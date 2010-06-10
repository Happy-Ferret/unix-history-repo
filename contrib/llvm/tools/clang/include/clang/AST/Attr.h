//===--- Attr.h - Classes for representing expressions ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Attr interface and subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_ATTR_H
#define LLVM_CLANG_AST_ATTR_H

#include "llvm/Support/Casting.h"
#include "llvm/ADT/StringRef.h"
#include <cassert>
#include <cstring>
#include <algorithm>
using llvm::dyn_cast;

namespace clang {
  class ASTContext;
  class IdentifierInfo;
  class ObjCInterfaceDecl;
}

// Defined in ASTContext.h
void *operator new(size_t Bytes, clang::ASTContext &C,
                   size_t Alignment = 16) throw ();

// It is good practice to pair new/delete operators.  Also, MSVC gives many
// warnings if a matching delete overload is not declared, even though the
// throw() spec guarantees it will not be implicitly called.
void operator delete(void *Ptr, clang::ASTContext &C, size_t)
              throw ();

namespace clang {

/// Attr - This represents one attribute.
class Attr {
public:
  enum Kind {
    Alias,
    Aligned,
    AlignMac68k,
    AlwaysInline,
    AnalyzerNoReturn, // Clang-specific.
    Annotate,
    AsmLabel, // Represent GCC asm label extension.
    BaseCheck,
    Blocks,
    CDecl,
    Cleanup,
    Const,
    Constructor,
    Deprecated,
    Destructor,
    FastCall,
    Final,
    Format,
    FormatArg,
    GNUInline,
    Hiding,
    IBOutletKind, // Clang-specific. Use "Kind" suffix to not conflict w/ macro.
    IBOutletCollectionKind, // Clang-specific.
    IBActionKind, // Clang-specific. Use "Kind" suffix to not conflict w/ macro.
    Malloc,
    MaxFieldAlignment,
    NoDebug,
    NoInline,
    NonNull,
    NoReturn,
    NoThrow,
    ObjCException,
    ObjCNSObject,
    Override,
    CFReturnsRetained,      // Clang/Checker-specific.
    CFReturnsNotRetained,   // Clang/Checker-specific.
    NSReturnsRetained,      // Clang/Checker-specific.
    NSReturnsNotRetained,   // Clang/Checker-specific.
    Overloadable, // Clang-specific
    Packed,
    Pure,
    Regparm,
    ReqdWorkGroupSize,   // OpenCL-specific
    Section,
    Sentinel,
    StdCall,
    ThisCall,
    TransparentUnion,
    Unavailable,
    Unused,
    Used,
    Visibility,
    WarnUnusedResult,
    Weak,
    WeakImport,
    WeakRef,

    FIRST_TARGET_ATTRIBUTE,
    DLLExport,
    DLLImport,
    MSP430Interrupt,
    X86ForceAlignArgPointer
  };

private:
  Attr *Next;
  Kind AttrKind;
  bool Inherited : 1;

protected:
  void* operator new(size_t bytes) throw() {
    assert(0 && "Attrs cannot be allocated with regular 'new'.");
    return 0;
  }
  void operator delete(void* data) throw() {
    assert(0 && "Attrs cannot be released with regular 'delete'.");
  }

protected:
  Attr(Kind AK) : Next(0), AttrKind(AK), Inherited(false) {}
  virtual ~Attr() {
    assert(Next == 0 && "Destroy didn't work");
  }
public:
  virtual void Destroy(ASTContext &C);

  /// \brief Whether this attribute should be merged to new
  /// declarations.
  virtual bool isMerged() const { return true; }

  Kind getKind() const { return AttrKind; }

  Attr *getNext() { return Next; }
  const Attr *getNext() const { return Next; }
  void setNext(Attr *next) { Next = next; }

  template<typename T> const T *getNext() const {
    for (const Attr *attr = getNext(); attr; attr = attr->getNext())
      if (const T *V = dyn_cast<T>(attr))
        return V;
    return 0;
  }

  bool isInherited() const { return Inherited; }
  void setInherited(bool value) { Inherited = value; }

  void addAttr(Attr *attr) {
    assert((attr != 0) && "addAttr(): attr is null");

    // FIXME: This doesn't preserve the order in any way.
    attr->Next = Next;
    Next = attr;
  }

  // Clone this attribute.
  virtual Attr* clone(ASTContext &C) const = 0;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *) { return true; }
};
  
class AttrWithString : public Attr {
private:
  const char *Str;
  unsigned StrLen;
protected:
  AttrWithString(Attr::Kind AK, ASTContext &C, llvm::StringRef s);
  llvm::StringRef getString() const { return llvm::StringRef(Str, StrLen); }
  void ReplaceString(ASTContext &C, llvm::StringRef newS);
public:
  virtual void Destroy(ASTContext &C);
};

#define DEF_SIMPLE_ATTR(ATTR)                                           \
class ATTR##Attr : public Attr {                                        \
public:                                                                 \
  ATTR##Attr() : Attr(ATTR) {}                                          \
  virtual Attr *clone(ASTContext &C) const;                             \
  static bool classof(const Attr *A) { return A->getKind() == ATTR; }   \
  static bool classof(const ATTR##Attr *A) { return true; }             \
}

DEF_SIMPLE_ATTR(Packed);

/// \brief Attribute for specifying a maximum field alignment; this is only
/// valid on record decls.
class MaxFieldAlignmentAttr : public Attr {
  unsigned Alignment;

public:
  MaxFieldAlignmentAttr(unsigned alignment)
    : Attr(MaxFieldAlignment), Alignment(alignment) {}

  /// getAlignment - The specified alignment in bits.
  unsigned getAlignment() const { return Alignment; }

  virtual Attr* clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) {
    return A->getKind() == MaxFieldAlignment;
  }
  static bool classof(const MaxFieldAlignmentAttr *A) { return true; }
};

DEF_SIMPLE_ATTR(AlignMac68k);

class AlignedAttr : public Attr {
  unsigned Alignment;
public:
  AlignedAttr(unsigned alignment)
    : Attr(Aligned), Alignment(alignment) {}

  /// getAlignment - The specified alignment in bits.
  unsigned getAlignment() const { return Alignment; }
  
  /// getMaxAlignment - Get the maximum alignment of attributes on this list.
  unsigned getMaxAlignment() const {
    const AlignedAttr *Next = getNext<AlignedAttr>();
    if (Next)
      return std::max(Next->getMaxAlignment(), Alignment);
    else
      return Alignment;
  }

  virtual Attr* clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) {
    return A->getKind() == Aligned;
  }
  static bool classof(const AlignedAttr *A) { return true; }
};

class AnnotateAttr : public AttrWithString {
public:
  AnnotateAttr(ASTContext &C, llvm::StringRef ann)
    : AttrWithString(Annotate, C, ann) {}

  llvm::StringRef getAnnotation() const { return getString(); }

  virtual Attr* clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) {
    return A->getKind() == Annotate;
  }
  static bool classof(const AnnotateAttr *A) { return true; }
};

class AsmLabelAttr : public AttrWithString {
public:
  AsmLabelAttr(ASTContext &C, llvm::StringRef L)
    : AttrWithString(AsmLabel, C, L) {}

  llvm::StringRef getLabel() const { return getString(); }

  virtual Attr* clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) {
    return A->getKind() == AsmLabel;
  }
  static bool classof(const AsmLabelAttr *A) { return true; }
};

DEF_SIMPLE_ATTR(AlwaysInline);

class AliasAttr : public AttrWithString {
public:
  AliasAttr(ASTContext &C, llvm::StringRef aliasee)
    : AttrWithString(Alias, C, aliasee) {}

  llvm::StringRef getAliasee() const { return getString(); }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == Alias; }
  static bool classof(const AliasAttr *A) { return true; }
};

class ConstructorAttr : public Attr {
  int priority;
public:
  ConstructorAttr(int p) : Attr(Constructor), priority(p) {}

  int getPriority() const { return priority; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == Constructor; }
  static bool classof(const ConstructorAttr *A) { return true; }
};

class DestructorAttr : public Attr {
  int priority;
public:
  DestructorAttr(int p) : Attr(Destructor), priority(p) {}

  int getPriority() const { return priority; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == Destructor; }
  static bool classof(const DestructorAttr *A) { return true; }
};

class IBOutletAttr : public Attr {
public:
  IBOutletAttr() : Attr(IBOutletKind) {}

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) {
    return A->getKind() == IBOutletKind;
  }
  static bool classof(const IBOutletAttr *A) { return true; }
};

class IBOutletCollectionAttr : public Attr {
  const ObjCInterfaceDecl *D;
public:
  IBOutletCollectionAttr(const ObjCInterfaceDecl *d = 0)
    : Attr(IBOutletCollectionKind), D(d) {}

  const ObjCInterfaceDecl *getClass() const { return D; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) {
    return A->getKind() == IBOutletCollectionKind;
  }
  static bool classof(const IBOutletCollectionAttr *A) { return true; }
};

class IBActionAttr : public Attr {
public:
  IBActionAttr() : Attr(IBActionKind) {}

  virtual Attr *clone(ASTContext &C) const;

    // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) {
    return A->getKind() == IBActionKind;
  }
  static bool classof(const IBActionAttr *A) { return true; }
};

DEF_SIMPLE_ATTR(AnalyzerNoReturn);
DEF_SIMPLE_ATTR(Deprecated);
DEF_SIMPLE_ATTR(Final);
DEF_SIMPLE_ATTR(GNUInline);
DEF_SIMPLE_ATTR(Malloc);
DEF_SIMPLE_ATTR(NoReturn);

class SectionAttr : public AttrWithString {
public:
  SectionAttr(ASTContext &C, llvm::StringRef N)
    : AttrWithString(Section, C, N) {}

  llvm::StringRef getName() const { return getString(); }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) {
    return A->getKind() == Section;
  }
  static bool classof(const SectionAttr *A) { return true; }
};

DEF_SIMPLE_ATTR(Unavailable);
DEF_SIMPLE_ATTR(Unused);
DEF_SIMPLE_ATTR(Used);
DEF_SIMPLE_ATTR(Weak);
DEF_SIMPLE_ATTR(WeakImport);
DEF_SIMPLE_ATTR(WeakRef);
DEF_SIMPLE_ATTR(NoThrow);
DEF_SIMPLE_ATTR(Const);
DEF_SIMPLE_ATTR(Pure);

class NonNullAttr : public Attr {
  unsigned* ArgNums;
  unsigned Size;
public:
  NonNullAttr(ASTContext &C, unsigned* arg_nums = 0, unsigned size = 0);

  virtual void Destroy(ASTContext &C);

  typedef const unsigned *iterator;
  iterator begin() const { return ArgNums; }
  iterator end() const { return ArgNums + Size; }
  unsigned size() const { return Size; }

  bool isNonNull(unsigned arg) const {
    return ArgNums ? std::binary_search(ArgNums, ArgNums+Size, arg) : true;
  }

  virtual Attr *clone(ASTContext &C) const;

  static bool classof(const Attr *A) { return A->getKind() == NonNull; }
  static bool classof(const NonNullAttr *A) { return true; }
};

class FormatAttr : public AttrWithString {
  int formatIdx, firstArg;
public:
  FormatAttr(ASTContext &C, llvm::StringRef type, int idx, int first)
    : AttrWithString(Format, C, type), formatIdx(idx), firstArg(first) {}

  llvm::StringRef getType() const { return getString(); }
  void setType(ASTContext &C, llvm::StringRef type);
  int getFormatIdx() const { return formatIdx; }
  int getFirstArg() const { return firstArg; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == Format; }
  static bool classof(const FormatAttr *A) { return true; }
};

class FormatArgAttr : public Attr {
  int formatIdx;
public:
  FormatArgAttr(int idx) : Attr(FormatArg), formatIdx(idx) {}
  int getFormatIdx() const { return formatIdx; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == FormatArg; }
  static bool classof(const FormatArgAttr *A) { return true; }
};

class SentinelAttr : public Attr {
  int sentinel, NullPos;
public:
  SentinelAttr(int sentinel_val, int nullPos) : Attr(Sentinel),
               sentinel(sentinel_val), NullPos(nullPos) {}
  int getSentinel() const { return sentinel; }
  int getNullPos() const { return NullPos; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == Sentinel; }
  static bool classof(const SentinelAttr *A) { return true; }
};

class VisibilityAttr : public Attr {
public:
  /// @brief An enumeration for the kinds of visibility of symbols.
  enum VisibilityTypes {
    DefaultVisibility = 0,
    HiddenVisibility,
    ProtectedVisibility
  };
private:
  VisibilityTypes VisibilityType;
public:
  VisibilityAttr(VisibilityTypes v) : Attr(Visibility),
                 VisibilityType(v) {}

  VisibilityTypes getVisibility() const { return VisibilityType; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == Visibility; }
  static bool classof(const VisibilityAttr *A) { return true; }
};

DEF_SIMPLE_ATTR(FastCall);
DEF_SIMPLE_ATTR(StdCall);
DEF_SIMPLE_ATTR(ThisCall);
DEF_SIMPLE_ATTR(CDecl);
DEF_SIMPLE_ATTR(TransparentUnion);
DEF_SIMPLE_ATTR(ObjCNSObject);
DEF_SIMPLE_ATTR(ObjCException);

class OverloadableAttr : public Attr {
public:
  OverloadableAttr() : Attr(Overloadable) { }

  virtual bool isMerged() const { return false; }

  virtual Attr *clone(ASTContext &C) const;

  static bool classof(const Attr *A) { return A->getKind() == Overloadable; }
  static bool classof(const OverloadableAttr *) { return true; }
};

class BlocksAttr : public Attr {
public:
  enum BlocksAttrTypes {
    ByRef = 0
  };
private:
  BlocksAttrTypes BlocksAttrType;
public:
  BlocksAttr(BlocksAttrTypes t) : Attr(Blocks), BlocksAttrType(t) {}

  BlocksAttrTypes getType() const { return BlocksAttrType; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == Blocks; }
  static bool classof(const BlocksAttr *A) { return true; }
};

class FunctionDecl;

class CleanupAttr : public Attr {
  FunctionDecl *FD;

public:
  CleanupAttr(FunctionDecl *fd) : Attr(Cleanup), FD(fd) {}

  const FunctionDecl *getFunctionDecl() const { return FD; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == Cleanup; }
  static bool classof(const CleanupAttr *A) { return true; }
};

DEF_SIMPLE_ATTR(NoDebug);
DEF_SIMPLE_ATTR(WarnUnusedResult);
DEF_SIMPLE_ATTR(NoInline);

class RegparmAttr : public Attr {
  unsigned NumParams;

public:
  RegparmAttr(unsigned np) : Attr(Regparm), NumParams(np) {}

  unsigned getNumParams() const { return NumParams; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == Regparm; }
  static bool classof(const RegparmAttr *A) { return true; }
};

class ReqdWorkGroupSizeAttr : public Attr {
  unsigned X, Y, Z;
public:
  ReqdWorkGroupSizeAttr(unsigned X, unsigned Y, unsigned Z)
  : Attr(ReqdWorkGroupSize), X(X), Y(Y), Z(Z) {}

  unsigned getXDim() const { return X; }
  unsigned getYDim() const { return Y; }
  unsigned getZDim() const { return Z; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) {
    return A->getKind() == ReqdWorkGroupSize;
  }
  static bool classof(const ReqdWorkGroupSizeAttr *A) { return true; }
};

// Checker-specific attributes.
DEF_SIMPLE_ATTR(CFReturnsNotRetained);
DEF_SIMPLE_ATTR(CFReturnsRetained);
DEF_SIMPLE_ATTR(NSReturnsNotRetained);
DEF_SIMPLE_ATTR(NSReturnsRetained);

// C++0x member checking attributes.
DEF_SIMPLE_ATTR(BaseCheck);
DEF_SIMPLE_ATTR(Hiding);
DEF_SIMPLE_ATTR(Override);

// Target-specific attributes
DEF_SIMPLE_ATTR(DLLImport);
DEF_SIMPLE_ATTR(DLLExport);

class MSP430InterruptAttr : public Attr {
  unsigned Number;

public:
  MSP430InterruptAttr(unsigned n) : Attr(MSP430Interrupt), Number(n) {}

  unsigned getNumber() const { return Number; }

  virtual Attr *clone(ASTContext &C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Attr *A) { return A->getKind() == MSP430Interrupt; }
  static bool classof(const MSP430InterruptAttr *A) { return true; }
};

DEF_SIMPLE_ATTR(X86ForceAlignArgPointer);

#undef DEF_SIMPLE_ATTR

}  // end namespace clang

#endif
