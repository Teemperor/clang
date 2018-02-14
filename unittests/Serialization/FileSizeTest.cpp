//===- unittest/Serialization/GenerationCounterTest.cpp -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Tests the generation counter in the ExternalASTSource.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/Basic/MemoryBufferCache.h"
#include "clang/Serialization/SerializationDiagnostic.h"
#include "clang/Serialization/ASTWriter.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/MultiplexExternalSemaSource.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include <sstream>

namespace clang {
  namespace {
    class FileSizeClient : public clang::DiagnosticConsumer {
    public:
      bool GotSizeError = false;
      virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                                    const Diagnostic &Info) {
        ASSERT_EQ(Info.getID(), diag::err_fe_pch_too_big);
        GotSizeError = true;
      }
    };
  }

TEST(ASTWriter, FileSizeTest) {
  // Create a real ASTContext.
  std::unique_ptr<ASTUnit> ASTUnit = tooling::buildASTFromCode("int main() {}");
  clang::Sema &S = ASTUnit->getSema();
  clang::ASTContext &C = ASTUnit->getASTContext();


  MemoryBufferCache Cache;

  std::string Prefix;
  Prefix.resize(8192, '_');

  for (unsigned i = 0; i < 65500; i++) {
    std::string Data = std::to_string(i);
    Prefix.replace(0, Data.length(), Data.c_str());
    auto D = C.buildImplicitTypedef(C.IntTy, Prefix);
    C.getTranslationUnitDecl()->addDecl(D);
  }

  llvm::SmallVector<char, 16000> buffer;
  llvm::BitstreamWriter stream(buffer);
  clang::ASTWriter writer(stream, buffer, Cache, /*Extensions=*/{});

  FileSizeClient Client;

  S.getDiagnostics().setClient(&Client, false);
  writer.WriteAST(S, "outputFile", nullptr, "");
  ASSERT_TRUE(Client.GotSizeError);
}

} // namespace clang
