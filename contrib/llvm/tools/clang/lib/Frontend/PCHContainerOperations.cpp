//===--- Frontend/PCHContainerOperations.cpp - PCH Containers ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines PCHContainerOperations and RawPCHContainerOperation.
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/PCHContainerOperations.h"
#include "clang/AST/ASTConsumer.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Lex/ModuleLoader.h"
using namespace clang;

PCHContainerOperations::~PCHContainerOperations() {}

namespace {

/// \brief A PCHContainerGenerator that writes out the PCH to a flat file.
class PCHContainerGenerator : public ASTConsumer {
  std::shared_ptr<PCHBuffer> Buffer;
  raw_pwrite_stream *OS;

public:
  PCHContainerGenerator(DiagnosticsEngine &Diags,
                        const HeaderSearchOptions &HSO,
                        const PreprocessorOptions &PPO, const TargetOptions &TO,
                        const LangOptions &LO, const std::string &MainFileName,
                        const std::string &OutputFileName,
                        llvm::raw_pwrite_stream *OS,
                        std::shared_ptr<PCHBuffer> Buffer)
      : Buffer(Buffer), OS(OS) {}

  virtual ~PCHContainerGenerator() {}

  void HandleTranslationUnit(ASTContext &Ctx) override {
    if (Buffer->IsComplete) {
      // Make sure it hits disk now.
      *OS << Buffer->Data;
      OS->flush();
    }
    // Free the space of the temporary buffer.
    llvm::SmallVector<char, 0> Empty;
    Buffer->Data = std::move(Empty);
  }
};
}

std::unique_ptr<ASTConsumer>
RawPCHContainerOperations::CreatePCHContainerGenerator(
    DiagnosticsEngine &Diags, const HeaderSearchOptions &HSO,
    const PreprocessorOptions &PPO, const TargetOptions &TO,
    const LangOptions &LO, const std::string &MainFileName,
    const std::string &OutputFileName, llvm::raw_pwrite_stream *OS,
    std::shared_ptr<PCHBuffer> Buffer) const {
  return llvm::make_unique<PCHContainerGenerator>(
      Diags, HSO, PPO, TO, LO, MainFileName, OutputFileName, OS, Buffer);
}

void RawPCHContainerOperations::ExtractPCH(
    llvm::MemoryBufferRef Buffer, llvm::BitstreamReader &StreamFile) const {
  StreamFile.init((const unsigned char *)Buffer.getBufferStart(),
                  (const unsigned char *)Buffer.getBufferEnd());
}
