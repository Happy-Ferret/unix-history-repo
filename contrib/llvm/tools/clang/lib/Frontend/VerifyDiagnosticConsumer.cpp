//===---- VerifyDiagnosticConsumer.cpp - Verifying Diagnostic Client ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a concrete diagnostic client, which buffers the diagnostic messages.
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/FileManager.h"
#include "clang/Frontend/VerifyDiagnosticConsumer.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/raw_ostream.h"
#include <cctype>

using namespace clang;
typedef VerifyDiagnosticConsumer::Directive Directive;
typedef VerifyDiagnosticConsumer::DirectiveList DirectiveList;
typedef VerifyDiagnosticConsumer::ExpectedData ExpectedData;

VerifyDiagnosticConsumer::VerifyDiagnosticConsumer(DiagnosticsEngine &_Diags)
  : Diags(_Diags),
    PrimaryClient(Diags.getClient()), OwnsPrimaryClient(Diags.ownsClient()),
    Buffer(new TextDiagnosticBuffer()), CurrentPreprocessor(0),
    ActiveSourceFiles(0)
{
  Diags.takeClient();
}

VerifyDiagnosticConsumer::~VerifyDiagnosticConsumer() {
  assert(!ActiveSourceFiles && "Incomplete parsing of source files!");
  assert(!CurrentPreprocessor && "CurrentPreprocessor should be invalid!");
  CheckDiagnostics();  
  Diags.takeClient();
  if (OwnsPrimaryClient)
    delete PrimaryClient;
}

#ifndef NDEBUG
namespace {
class VerifyFileTracker : public PPCallbacks {
  typedef VerifyDiagnosticConsumer::FilesParsedForDirectivesSet ListType;
  ListType &FilesList;
  SourceManager &SM;

public:
  VerifyFileTracker(ListType &FilesList, SourceManager &SM)
    : FilesList(FilesList), SM(SM) { }

  /// \brief Hook into the preprocessor and update the list of parsed
  /// files when the preprocessor indicates a new file is entered.
  virtual void FileChanged(SourceLocation Loc, FileChangeReason Reason,
                           SrcMgr::CharacteristicKind FileType,
                           FileID PrevFID) {
    if (const FileEntry *E = SM.getFileEntryForID(SM.getFileID(Loc)))
      FilesList.insert(E);
  }
};
} // End anonymous namespace.
#endif

// DiagnosticConsumer interface.

void VerifyDiagnosticConsumer::BeginSourceFile(const LangOptions &LangOpts,
                                               const Preprocessor *PP) {
  // Attach comment handler on first invocation.
  if (++ActiveSourceFiles == 1) {
    if (PP) {
      CurrentPreprocessor = PP;
      const_cast<Preprocessor*>(PP)->addCommentHandler(this);
#ifndef NDEBUG
      VerifyFileTracker *V = new VerifyFileTracker(FilesParsedForDirectives,
                                                   PP->getSourceManager());
      const_cast<Preprocessor*>(PP)->addPPCallbacks(V);
#endif
    }
  }

  assert((!PP || CurrentPreprocessor == PP) && "Preprocessor changed!");
  PrimaryClient->BeginSourceFile(LangOpts, PP);
}

void VerifyDiagnosticConsumer::EndSourceFile() {
  assert(ActiveSourceFiles && "No active source files!");
  PrimaryClient->EndSourceFile();

  // Detach comment handler once last active source file completed.
  if (--ActiveSourceFiles == 0) {
    if (CurrentPreprocessor)
      const_cast<Preprocessor*>(CurrentPreprocessor)->removeCommentHandler(this);

    // Check diagnostics once last file completed.
    CheckDiagnostics();
    CurrentPreprocessor = 0;
  }
}

void VerifyDiagnosticConsumer::HandleDiagnostic(
      DiagnosticsEngine::Level DiagLevel, const Diagnostic &Info) {
#ifndef NDEBUG
  if (Info.hasSourceManager()) {
    FileID FID = Info.getSourceManager().getFileID(Info.getLocation());
    if (!FID.isInvalid())
      FilesWithDiagnostics.insert(FID);
  }
#endif
  // Send the diagnostic to the buffer, we will check it once we reach the end
  // of the source file (or are destructed).
  Buffer->HandleDiagnostic(DiagLevel, Info);
}

//===----------------------------------------------------------------------===//
// Checking diagnostics implementation.
//===----------------------------------------------------------------------===//

typedef TextDiagnosticBuffer::DiagList DiagList;
typedef TextDiagnosticBuffer::const_iterator const_diag_iterator;

namespace {

/// StandardDirective - Directive with string matching.
///
class StandardDirective : public Directive {
public:
  StandardDirective(SourceLocation DirectiveLoc, SourceLocation DiagnosticLoc,
                    StringRef Text, unsigned Min, unsigned Max)
    : Directive(DirectiveLoc, DiagnosticLoc, Text, Min, Max) { }

  virtual bool isValid(std::string &Error) {
    // all strings are considered valid; even empty ones
    return true;
  }

  virtual bool match(StringRef S) {
    return S.find(Text) != StringRef::npos;
  }
};

/// RegexDirective - Directive with regular-expression matching.
///
class RegexDirective : public Directive {
public:
  RegexDirective(SourceLocation DirectiveLoc, SourceLocation DiagnosticLoc,
                 StringRef Text, unsigned Min, unsigned Max)
    : Directive(DirectiveLoc, DiagnosticLoc, Text, Min, Max), Regex(Text) { }

  virtual bool isValid(std::string &Error) {
    if (Regex.isValid(Error))
      return true;
    return false;
  }

  virtual bool match(StringRef S) {
    return Regex.match(S);
  }

private:
  llvm::Regex Regex;
};

class ParseHelper
{
public:
  ParseHelper(StringRef S)
    : Begin(S.begin()), End(S.end()), C(Begin), P(Begin), PEnd(NULL) { }

  // Return true if string literal is next.
  bool Next(StringRef S) {
    P = C;
    PEnd = C + S.size();
    if (PEnd > End)
      return false;
    return !memcmp(P, S.data(), S.size());
  }

  // Return true if number is next.
  // Output N only if number is next.
  bool Next(unsigned &N) {
    unsigned TMP = 0;
    P = C;
    for (; P < End && P[0] >= '0' && P[0] <= '9'; ++P) {
      TMP *= 10;
      TMP += P[0] - '0';
    }
    if (P == C)
      return false;
    PEnd = P;
    N = TMP;
    return true;
  }

  // Return true if string literal is found.
  // When true, P marks begin-position of S in content.
  bool Search(StringRef S) {
    P = std::search(C, End, S.begin(), S.end());
    PEnd = P + S.size();
    return P != End;
  }

  // Advance 1-past previous next/search.
  // Behavior is undefined if previous next/search failed.
  bool Advance() {
    C = PEnd;
    return C < End;
  }

  // Skip zero or more whitespace.
  void SkipWhitespace() {
    for (; C < End && isspace(*C); ++C)
      ;
  }

  // Return true if EOF reached.
  bool Done() {
    return !(C < End);
  }

  const char * const Begin; // beginning of expected content
  const char * const End;   // end of expected content (1-past)
  const char *C;            // position of next char in content
  const char *P;

private:
  const char *PEnd; // previous next/search subject end (1-past)
};

} // namespace anonymous

/// ParseDirective - Go through the comment and see if it indicates expected
/// diagnostics. If so, then put them in the appropriate directive list.
///
/// Returns true if any valid directives were found.
static bool ParseDirective(StringRef S, ExpectedData *ED, SourceManager &SM,
                           SourceLocation Pos, DiagnosticsEngine &Diags) {
  // A single comment may contain multiple directives.
  bool FoundDirective = false;
  for (ParseHelper PH(S); !PH.Done();) {
    // Search for token: expected
    if (!PH.Search("expected"))
      break;
    PH.Advance();

    // Next token: -
    if (!PH.Next("-"))
      continue;
    PH.Advance();

    // Next token: { error | warning | note }
    DirectiveList* DL = NULL;
    if (PH.Next("error"))
      DL = ED ? &ED->Errors : NULL;
    else if (PH.Next("warning"))
      DL = ED ? &ED->Warnings : NULL;
    else if (PH.Next("note"))
      DL = ED ? &ED->Notes : NULL;
    else
      continue;
    PH.Advance();

    // If a directive has been found but we're not interested
    // in storing the directive information, return now.
    if (!DL)
      return true;

    // Default directive kind.
    bool RegexKind = false;
    const char* KindStr = "string";

    // Next optional token: -
    if (PH.Next("-re")) {
      PH.Advance();
      RegexKind = true;
      KindStr = "regex";
    }

    // Next optional token: @
    SourceLocation ExpectedLoc;
    if (!PH.Next("@")) {
      ExpectedLoc = Pos;
    } else {
      PH.Advance();
      unsigned Line = 0;
      bool FoundPlus = PH.Next("+");
      if (FoundPlus || PH.Next("-")) {
        // Relative to current line.
        PH.Advance();
        bool Invalid = false;
        unsigned ExpectedLine = SM.getSpellingLineNumber(Pos, &Invalid);
        if (!Invalid && PH.Next(Line) && (FoundPlus || Line < ExpectedLine)) {
          if (FoundPlus) ExpectedLine += Line;
          else ExpectedLine -= Line;
          ExpectedLoc = SM.translateLineCol(SM.getFileID(Pos), ExpectedLine, 1);
        }
      } else {
        // Absolute line number.
        if (PH.Next(Line) && Line > 0)
          ExpectedLoc = SM.translateLineCol(SM.getFileID(Pos), Line, 1);
      }

      if (ExpectedLoc.isInvalid()) {
        Diags.Report(Pos.getLocWithOffset(PH.C-PH.Begin),
                     diag::err_verify_missing_line) << KindStr;
        continue;
      }
      PH.Advance();
    }

    // Skip optional whitespace.
    PH.SkipWhitespace();

    // Next optional token: positive integer or a '+'.
    unsigned Min = 1;
    unsigned Max = 1;
    if (PH.Next(Min)) {
      PH.Advance();
      // A positive integer can be followed by a '+' meaning min
      // or more, or by a '-' meaning a range from min to max.
      if (PH.Next("+")) {
        Max = Directive::MaxCount;
        PH.Advance();
      } else if (PH.Next("-")) {
        PH.Advance();
        if (!PH.Next(Max) || Max < Min) {
          Diags.Report(Pos.getLocWithOffset(PH.C-PH.Begin),
                       diag::err_verify_invalid_range) << KindStr;
          continue;
        }
        PH.Advance();
      } else {
        Max = Min;
      }
    } else if (PH.Next("+")) {
      // '+' on its own means "1 or more".
      Max = Directive::MaxCount;
      PH.Advance();
    }

    // Skip optional whitespace.
    PH.SkipWhitespace();

    // Next token: {{
    if (!PH.Next("{{")) {
      Diags.Report(Pos.getLocWithOffset(PH.C-PH.Begin),
                   diag::err_verify_missing_start) << KindStr;
      continue;
    }
    PH.Advance();
    const char* const ContentBegin = PH.C; // mark content begin

    // Search for token: }}
    if (!PH.Search("}}")) {
      Diags.Report(Pos.getLocWithOffset(PH.C-PH.Begin),
                   diag::err_verify_missing_end) << KindStr;
      continue;
    }
    const char* const ContentEnd = PH.P; // mark content end
    PH.Advance();

    // Build directive text; convert \n to newlines.
    std::string Text;
    StringRef NewlineStr = "\\n";
    StringRef Content(ContentBegin, ContentEnd-ContentBegin);
    size_t CPos = 0;
    size_t FPos;
    while ((FPos = Content.find(NewlineStr, CPos)) != StringRef::npos) {
      Text += Content.substr(CPos, FPos-CPos);
      Text += '\n';
      CPos = FPos + NewlineStr.size();
    }
    if (Text.empty())
      Text.assign(ContentBegin, ContentEnd);

    // Construct new directive.
    Directive *D = Directive::create(RegexKind, Pos, ExpectedLoc, Text,
                                     Min, Max);
    std::string Error;
    if (D->isValid(Error)) {
      DL->push_back(D);
      FoundDirective = true;
    } else {
      Diags.Report(Pos.getLocWithOffset(ContentBegin-PH.Begin),
                   diag::err_verify_invalid_content)
        << KindStr << Error;
    }
  }

  return FoundDirective;
}

/// HandleComment - Hook into the preprocessor and extract comments containing
///  expected errors and warnings.
bool VerifyDiagnosticConsumer::HandleComment(Preprocessor &PP,
                                             SourceRange Comment) {
  SourceManager &SM = PP.getSourceManager();
  SourceLocation CommentBegin = Comment.getBegin();

  const char *CommentRaw = SM.getCharacterData(CommentBegin);
  StringRef C(CommentRaw, SM.getCharacterData(Comment.getEnd()) - CommentRaw);

  if (C.empty())
    return false;

  // Fold any "\<EOL>" sequences
  size_t loc = C.find('\\');
  if (loc == StringRef::npos) {
    ParseDirective(C, &ED, SM, CommentBegin, PP.getDiagnostics());
    return false;
  }

  std::string C2;
  C2.reserve(C.size());

  for (size_t last = 0;; loc = C.find('\\', last)) {
    if (loc == StringRef::npos || loc == C.size()) {
      C2 += C.substr(last);
      break;
    }
    C2 += C.substr(last, loc-last);
    last = loc + 1;

    if (C[last] == '\n' || C[last] == '\r') {
      ++last;

      // Escape \r\n  or \n\r, but not \n\n.
      if (last < C.size())
        if (C[last] == '\n' || C[last] == '\r')
          if (C[last] != C[last-1])
            ++last;
    } else {
      // This was just a normal backslash.
      C2 += '\\';
    }
  }

  if (!C2.empty())
    ParseDirective(C2, &ED, SM, CommentBegin, PP.getDiagnostics());
  return false;
}

#ifndef NDEBUG
/// \brief Lex the specified source file to determine whether it contains
/// any expected-* directives.  As a Lexer is used rather than a full-blown
/// Preprocessor, directives inside skipped #if blocks will still be found.
///
/// \return true if any directives were found.
static bool findDirectives(const Preprocessor &PP, FileID FID) {
  // Create a raw lexer to pull all the comments out of FID.
  if (FID.isInvalid())
    return false;

  SourceManager& SM = PP.getSourceManager();
  // Create a lexer to lex all the tokens of the main file in raw mode.
  const llvm::MemoryBuffer *FromFile = SM.getBuffer(FID);
  Lexer RawLex(FID, FromFile, SM, PP.getLangOpts());

  // Return comments as tokens, this is how we find expected diagnostics.
  RawLex.SetCommentRetentionState(true);

  Token Tok;
  Tok.setKind(tok::comment);
  bool Found = false;
  while (Tok.isNot(tok::eof)) {
    RawLex.Lex(Tok);
    if (!Tok.is(tok::comment)) continue;

    std::string Comment = PP.getSpelling(Tok);
    if (Comment.empty()) continue;

    // Find all expected errors/warnings/notes.
    Found |= ParseDirective(Comment, 0, SM, Tok.getLocation(),
                            PP.getDiagnostics());
  }
  return Found;
}
#endif // !NDEBUG

/// \brief Takes a list of diagnostics that have been generated but not matched
/// by an expected-* directive and produces a diagnostic to the user from this.
static unsigned PrintUnexpected(DiagnosticsEngine &Diags, SourceManager *SourceMgr,
                                const_diag_iterator diag_begin,
                                const_diag_iterator diag_end,
                                const char *Kind) {
  if (diag_begin == diag_end) return 0;

  SmallString<256> Fmt;
  llvm::raw_svector_ostream OS(Fmt);
  for (const_diag_iterator I = diag_begin, E = diag_end; I != E; ++I) {
    if (I->first.isInvalid() || !SourceMgr)
      OS << "\n  (frontend)";
    else
      OS << "\n  Line " << SourceMgr->getPresumedLineNumber(I->first);
    OS << ": " << I->second;
  }

  Diags.Report(diag::err_verify_inconsistent_diags).setForceEmit()
    << Kind << /*Unexpected=*/true << OS.str();
  return std::distance(diag_begin, diag_end);
}

/// \brief Takes a list of diagnostics that were expected to have been generated
/// but were not and produces a diagnostic to the user from this.
static unsigned PrintExpected(DiagnosticsEngine &Diags, SourceManager &SourceMgr,
                              DirectiveList &DL, const char *Kind) {
  if (DL.empty())
    return 0;

  SmallString<256> Fmt;
  llvm::raw_svector_ostream OS(Fmt);
  for (DirectiveList::iterator I = DL.begin(), E = DL.end(); I != E; ++I) {
    Directive &D = **I;
    OS << "\n  Line " << SourceMgr.getPresumedLineNumber(D.DiagnosticLoc);
    if (D.DirectiveLoc != D.DiagnosticLoc)
      OS << " (directive at "
         << SourceMgr.getFilename(D.DirectiveLoc) << ":"
         << SourceMgr.getPresumedLineNumber(D.DirectiveLoc) << ")";
    OS << ": " << D.Text;
  }

  Diags.Report(diag::err_verify_inconsistent_diags).setForceEmit()
    << Kind << /*Unexpected=*/false << OS.str();
  return DL.size();
}

/// CheckLists - Compare expected to seen diagnostic lists and return the
/// the difference between them.
///
static unsigned CheckLists(DiagnosticsEngine &Diags, SourceManager &SourceMgr,
                           const char *Label,
                           DirectiveList &Left,
                           const_diag_iterator d2_begin,
                           const_diag_iterator d2_end) {
  DirectiveList LeftOnly;
  DiagList Right(d2_begin, d2_end);

  for (DirectiveList::iterator I = Left.begin(), E = Left.end(); I != E; ++I) {
    Directive& D = **I;
    unsigned LineNo1 = SourceMgr.getPresumedLineNumber(D.DiagnosticLoc);

    for (unsigned i = 0; i < D.Max; ++i) {
      DiagList::iterator II, IE;
      for (II = Right.begin(), IE = Right.end(); II != IE; ++II) {
        unsigned LineNo2 = SourceMgr.getPresumedLineNumber(II->first);
        if (LineNo1 != LineNo2)
          continue;

        const std::string &RightText = II->second;
        if (D.match(RightText))
          break;
      }
      if (II == IE) {
        // Not found.
        if (i >= D.Min) break;
        LeftOnly.push_back(*I);
      } else {
        // Found. The same cannot be found twice.
        Right.erase(II);
      }
    }
  }
  // Now all that's left in Right are those that were not matched.
  unsigned num = PrintExpected(Diags, SourceMgr, LeftOnly, Label);
  num += PrintUnexpected(Diags, &SourceMgr, Right.begin(), Right.end(), Label);
  return num;
}

/// CheckResults - This compares the expected results to those that
/// were actually reported. It emits any discrepencies. Return "true" if there
/// were problems. Return "false" otherwise.
///
static unsigned CheckResults(DiagnosticsEngine &Diags, SourceManager &SourceMgr,
                             const TextDiagnosticBuffer &Buffer,
                             ExpectedData &ED) {
  // We want to capture the delta between what was expected and what was
  // seen.
  //
  //   Expected \ Seen - set expected but not seen
  //   Seen \ Expected - set seen but not expected
  unsigned NumProblems = 0;

  // See if there are error mismatches.
  NumProblems += CheckLists(Diags, SourceMgr, "error", ED.Errors,
                            Buffer.err_begin(), Buffer.err_end());

  // See if there are warning mismatches.
  NumProblems += CheckLists(Diags, SourceMgr, "warning", ED.Warnings,
                            Buffer.warn_begin(), Buffer.warn_end());

  // See if there are note mismatches.
  NumProblems += CheckLists(Diags, SourceMgr, "note", ED.Notes,
                            Buffer.note_begin(), Buffer.note_end());

  return NumProblems;
}

void VerifyDiagnosticConsumer::CheckDiagnostics() {
  // Ensure any diagnostics go to the primary client.
  bool OwnsCurClient = Diags.ownsClient();
  DiagnosticConsumer *CurClient = Diags.takeClient();
  Diags.setClient(PrimaryClient, false);

  // If we have a preprocessor, scan the source for expected diagnostic
  // markers. If not then any diagnostics are unexpected.
  if (CurrentPreprocessor) {
    SourceManager &SM = CurrentPreprocessor->getSourceManager();

#ifndef NDEBUG
    // In a debug build, scan through any files that may have been missed
    // during parsing and issue a fatal error if directives are contained
    // within these files.  If a fatal error occurs, this suggests that
    // this file is being parsed separately from the main file.
    HeaderSearch &HS = CurrentPreprocessor->getHeaderSearchInfo();
    for (FilesWithDiagnosticsSet::iterator I = FilesWithDiagnostics.begin(),
                                         End = FilesWithDiagnostics.end();
            I != End; ++I) {
      const FileEntry *E = SM.getFileEntryForID(*I);
      // Don't check files already parsed or those handled as modules.
      if (E && (FilesParsedForDirectives.count(E)
                  || HS.findModuleForHeader(E)))
        continue;

      if (findDirectives(*CurrentPreprocessor, *I))
        llvm::report_fatal_error(Twine("-verify directives found after rather"
                                       " than during normal parsing of ",
                                 StringRef(E ? E->getName() : "(unknown)")));
    }
#endif

    // Check that the expected diagnostics occurred.
    NumErrors += CheckResults(Diags, SM, *Buffer, ED);
  } else {
    NumErrors += (PrintUnexpected(Diags, 0, Buffer->err_begin(),
                                  Buffer->err_end(), "error") +
                  PrintUnexpected(Diags, 0, Buffer->warn_begin(),
                                  Buffer->warn_end(), "warn") +
                  PrintUnexpected(Diags, 0, Buffer->note_begin(),
                                  Buffer->note_end(), "note"));
  }

  Diags.takeClient();
  Diags.setClient(CurClient, OwnsCurClient);

  // Reset the buffer, we have processed all the diagnostics in it.
  Buffer.reset(new TextDiagnosticBuffer());
  ED.Errors.clear();
  ED.Warnings.clear();
  ED.Notes.clear();
}

DiagnosticConsumer *
VerifyDiagnosticConsumer::clone(DiagnosticsEngine &Diags) const {
  if (!Diags.getClient())
    Diags.setClient(PrimaryClient->clone(Diags));
  
  return new VerifyDiagnosticConsumer(Diags);
}

Directive *Directive::create(bool RegexKind, SourceLocation DirectiveLoc,
                             SourceLocation DiagnosticLoc, StringRef Text,
                             unsigned Min, unsigned Max) {
  if (RegexKind)
    return new RegexDirective(DirectiveLoc, DiagnosticLoc, Text, Min, Max);
  return new StandardDirective(DirectiveLoc, DiagnosticLoc, Text, Min, Max);
}
