//===- unittest/Serialization/FileSizeTest.cpp -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Tests if we give an error when we hit the AST file size limit.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/Basic/MemoryBufferCache.h"
#include "clang/Sema/Sema.h"
#include "clang/Serialization/ASTWriter.h"
#include "clang/Serialization/SerializationDiagnostic.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "gtest/gtest.h"
#include <sstream>

namespace clang {
namespace {
class FileSizeClient : public clang::DiagnosticConsumer {
public:
  bool GotSizeError = false;
  virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                                const Diagnostic &Info) {
    // Thath's the only diagnostic we should see during this test.
    ASSERT_EQ(Info.getID(), diag::err_fe_pch_too_big);
    GotSizeError = true;
  }
};
} // namespace

TEST(ASTWriter, FileSizeLimit) {
  // Create some empty AST we can use for this test.
  std::unique_ptr<ASTUnit> ASTUnit = tooling::buildASTFromCode("");
  clang::Sema &S = ASTUnit->getSema();
  clang::ASTContext &C = ASTUnit->getASTContext();

  // Fill the AST with typedef decls until we have at least 512MiB of data which
  // is the AST file size limit.
  std::string Prefix;
  Prefix.resize(8192, '_');
  unsigned NumberOfTypedefs = 65500; // 65500 * 8K = 512MiB.

  for (unsigned i = 0; i < NumberOfTypedefs; i++) {
    // Create a typedef with a unique name '_i_____' and add it to the AST.
    std::string Data = std::to_string(i);
    Prefix.replace(0, Data.length(), Data.c_str());
    auto D = C.buildImplicitTypedef(C.IntTy, Prefix);
    C.getTranslationUnitDecl()->addDecl(D);
  }

  // Attach a custom diagnostic client to our Sema that we can verify if the
  // ASTWriter creates the diagnostic we need.
  FileSizeClient Client;
  S.getDiagnostics().setClient(&Client, false);

  llvm::SmallVector<char, 1> Buffer;
  Buffer.reserve(560000000ul);
  llvm::BitstreamWriter Stream(Buffer);

  // Write our AST to a buffer with the ASTWriter.
  MemoryBufferCache Cache;
  clang::ASTWriter Writer(Stream, Buffer, Cache, /*Extensions=*/{});

  Writer.WriteAST(S, "out.pcm", nullptr, "");
  ASSERT_TRUE(Client.GotSizeError);
}

} // namespace clang
