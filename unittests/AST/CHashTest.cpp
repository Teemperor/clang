//===- unittests/AST/DataCollectionTest.cpp -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains tests for the DataCollection module.
//
// They work by hashing the collected data of two nodes and asserting that the
// hash values are equal iff the nodes are considered equal.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/CHashVisitor.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"
#include <memory>

using namespace clang;
using namespace tooling;


class CHashConsumer : public ASTConsumer {
    CompilerInstance &CI;
    llvm::MD5::MD5Result *ASTHash;

public:

    CHashConsumer(CompilerInstance &CI, llvm::MD5::MD5Result *ASTHash)
        : CI(CI), ASTHash(ASTHash){}

    virtual void HandleTranslationUnit(clang::ASTContext &Context) override {
        TranslationUnitDecl *TU = Context.getTranslationUnitDecl();

        // Traversing the translation unit decl via a RecursiveASTVisitor
        // will visit all nodes in the AST.
        CHashVisitor<> Visitor(Context);
        Visitor.TraverseDecl(TU);
        // Copy Away the resulting hash
        *ASTHash = *Visitor.getHash(TU);

    }

    ~CHashConsumer() override {}
};

struct CHashAction : public ASTFrontendAction {
    llvm::MD5::MD5Result *Hash;

    CHashAction(llvm::MD5::MD5Result *Hash) : Hash(Hash) {}

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   StringRef) override {
        return std::unique_ptr<ASTConsumer>(new CHashConsumer(CI, Hash));
    }
};

static testing::AssertionResult
isASTHashEqual(StringRef Code1, StringRef Code2) {
    llvm::MD5::MD5Result Hash1, Hash2;
    if (!runToolOnCode(new CHashAction(&Hash1), Code1)) {
        return testing::AssertionFailure()
            << "Parsing error in (A)\"" << Code1.str() << "\"";
    }
    if (!runToolOnCode(new CHashAction(&Hash2), Code2)) {
        return testing::AssertionFailure()
            << "Parsing error in (B) \"" << Code2.str() << "\"";
    }
    return testing::AssertionResult(Hash1 == Hash2);
}

TEST(CHashVisitor, TestRecordTypes) {
    ASSERT_TRUE(isASTHashEqual( // Unused struct
                     "struct foobar { int a0; char a1; unsigned long a2; };",
                     "struct foobar { int a0; char a1;};"
                     ));

}

TEST(CHashVisitor, TestSourceStructure) {
    ASSERT_FALSE(isASTHashEqual(
                     "void foo() { int c; if (0) { c = 1; } }",
                     "void foo() { int c; if (0) { } c = 1; }"));

    ASSERT_FALSE(isASTHashEqual(
                     "void f1() {} void f2() {       }",
                     "void f1() {} void f2() { f1(); }"));
}
