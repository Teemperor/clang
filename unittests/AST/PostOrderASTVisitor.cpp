//===- unittests/AST/PostOrderASTVisitor.cpp - Declaration printer tests --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains tests for the post-order traversing functionality
// of RecursiveASTVisitor.
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

namespace {

  class RecordingVisitor
    : public RecursiveASTVisitor<RecordingVisitor> {

    bool VisitPostOrder;
  public:
    explicit RecordingVisitor(bool VisitPostOrder)
      : VisitPostOrder(VisitPostOrder) {
    }

    // List of visited nodes during traversal.
    std::vector<std::string> VisitedNodes;

    bool shouldTraversePostOrder() const { return VisitPostOrder; }

    bool PostVisitBinaryOperator(BinaryOperator *Op) {
      VisitedNodes.push_back(Op->getOpcodeStr());
      return true;
    }

    bool PostVisitIntegerLiteral(IntegerLiteral *Lit) {
      VisitedNodes.push_back(Lit->getValue().toString(10, false));
      return true;
    }

    bool PostVisitCXXMethodDecl(CXXMethodDecl *D) {
      VisitedNodes.push_back(D->getQualifiedNameAsString());
      return true;
    }

    bool PostVisitReturnStmt(Stmt *S) {
      VisitedNodes.push_back("return");
      return true;
    }

    bool PostVisitCXXRecordDecl(CXXRecordDecl *Declaration) {
      VisitedNodes.push_back(Declaration->getQualifiedNameAsString());
      return true;
    }

    bool PostVisitTemplateTypeParmType(TemplateTypeParmType *T) {
      VisitedNodes.push_back(T->getDecl()->getQualifiedNameAsString());
      return true;
    }
  };

}

TEST(RecursiveASTVisitor, PostOrderTraversal) {
  auto ASTUnit = tooling::buildASTFromCode(
    "template <class T> class A {"
    "  class B {"
    "    int foo() { return 1 + 2; }"
    "  };"
    "};"
  );
  auto TU = ASTUnit->getASTContext().getTranslationUnitDecl();
  // We traverse the translation unit and store all
  // visited nodes.
  RecordingVisitor Visitor(true);
  Visitor.TraverseTranslationUnitDecl(TU);

  std::vector<std::string> expected = {
    "1", "2", "+", "return", "A::B::foo", "A::B", "A", "A::T"
  };
  // Compare the list of actually visited nodes
  // with the expected list of visited nodes.
  ASSERT_EQ(expected.size(), Visitor.VisitedNodes.size());
  for (std::size_t I = 0; I < expected.size(); I++) {
    ASSERT_EQ(expected[I], Visitor.VisitedNodes[I]);
  }
}

TEST(RecursiveASTVisitor, DeactivatePostOrderTraversal) {
  auto ASTUnit = tooling::buildASTFromCode(
    "template <class T> class A {"
    "  class B {"
    "    int foo() { return 1 + 2; }"
    "  };"
    "};"
  );
  auto TU = ASTUnit->getASTContext().getTranslationUnitDecl();
  // We try to traverse the translation unit but with deactivated
  // post order calls.
  RecordingVisitor Visitor(false);
  Visitor.TraverseTranslationUnitDecl(TU);

  // We deactivated postorder traversal, so we shouldn't have
  // recorded any nodes.
  ASSERT_TRUE(Visitor.VisitedNodes.empty());
}
