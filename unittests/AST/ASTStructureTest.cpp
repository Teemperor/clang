//===- unittests/AST/ASTStructureTest.cpp --- AST structure hasing tests-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains tests for ASTStructure.
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"
#include "clang/AST/ASTStructure.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

class FunctionFinder : public RecursiveASTVisitor<FunctionFinder> {
  std::string FunctionName;
  FunctionDecl *FoundFunction = nullptr;
public:
  FunctionFinder (const std::string& FunctionName)
    : FunctionName(FunctionName) {
  }

  FunctionDecl *getDecl() {
    return FoundFunction;
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    if (D->getQualifiedNameAsString() == FunctionName) {
      FoundFunction = D;
      return false;
    }
    return true;
  }
};

bool isHashed(const std::string& DeclName, const std::string& code) {
  auto ASTUnit = tooling::buildASTFromCode(code);
  ASTContext& ASTContext = ASTUnit->getASTContext();

  auto Hash = ASTStructure(ASTUnit->getASTContext());

  FunctionFinder Finder(DeclName);
  Finder.TraverseTranslationUnitDecl(ASTContext.getTranslationUnitDecl());

  assert(Finder.getDecl());
  return Hash.findHash(Finder.getDecl()->getBody()).Success;
}

class FindStmt : public RecursiveASTVisitor<FindStmt> {
  Stmt::StmtClass NeededStmtClass;
  Stmt *FoundStmt = nullptr;
public:
  FindStmt(Stmt::StmtClass NeededStmtClass)
    : NeededStmtClass(NeededStmtClass) {
  }

  Stmt *getStmt() {
    return FoundStmt;
  }

  bool VisitStmt(Stmt *S) {
    if (S->getStmtClass() == NeededStmtClass) {
      FoundStmt = S;
      return false;
    }
    return true;
  }
};

bool isHashed(Stmt::StmtClass NeededStmtClass, const std::string& code) {
  auto ASTUnit = tooling::buildASTFromCode(code);
  ASTContext& ASTContext = ASTUnit->getASTContext();

  auto Hash = ASTStructure(ASTUnit->getASTContext());

  FindStmt Finder(NeededStmtClass);
  Finder.TraverseTranslationUnitDecl(ASTContext.getTranslationUnitDecl());

  assert(Finder.getStmt());
  return Hash.findHash(Finder.getStmt()).Success;
}

bool compareStructure(const std::string& DeclNameA,
                      const std::string& DeclNameB,
                      const std::string& codeA,
                      const std::string& codeB) {
  auto ASTUnitA = tooling::buildASTFromCodeWithArgs(codeA,
      {"-std=c++1z", "-fms-extensions"});
  auto ASTUnitB = tooling::buildASTFromCodeWithArgs(codeB,
      {"-std=c++1z", "-fms-extensions"});

  auto HashA = ASTStructure(ASTUnitA->getASTContext());
  auto HashB = ASTStructure(ASTUnitB->getASTContext());

  FunctionFinder FinderA(DeclNameA);
  FinderA.TraverseTranslationUnitDecl(ASTUnitA->getASTContext().getTranslationUnitDecl());
  assert(FinderA.getDecl());

  FunctionFinder FinderB(DeclNameB);
  FinderB.TraverseTranslationUnitDecl(ASTUnitB->getASTContext().getTranslationUnitDecl());
  assert(FinderB.getDecl());


  auto HashSearchA = HashA.findHash(FinderA.getDecl()->getBody());
  auto HashSearchB = HashB.findHash(FinderB.getDecl()->getBody());
  assert(HashSearchA.Success);
  assert(HashSearchB.Success);
  return HashSearchA.Hash == HashSearchB.Hash;
}


bool compareStmt(const std::string& codeA,
                 const std::string& codeB) {
  return compareStructure("x", "x",
                          "void x() { " + codeA + "}",
                          "void x() { " + codeB + "}");
}

TEST(ASTStructure, IfStmt) {
  ASSERT_TRUE(compareStmt(
      "if (true) {}",
      "if (false) {}"
  ));
  ASSERT_FALSE(compareStmt(
      "if (true) { int x; }",
      "if (false) {}"
  ));
  ASSERT_FALSE(compareStmt(
      "if (int y = 0) {}",
      "if (false) {}"
  ));
}

TEST(ASTStructure, StmtExpr) {
  ASSERT_TRUE(compareStmt(
      "int v = ({int x = 4; x;});",
      "int v = ({int y = 5; y;});"
  ));
  ASSERT_FALSE(compareStmt(
      "int v = ({int x = 4 + 4; x;});",
      "int v = ({int y = 5; y;});"
  ));
  ASSERT_FALSE(compareStmt(
      "int v = ({int x = 5; x;});",
      "int v = ({int y = 5; y++; y;});"
  ));
}


TEST(ASTStructure, MSDependentExistsStmt) {
  ASSERT_FALSE(compareStructure("x", "x",
      R"test(
      template<typename T>
      void x(T &t) {
        __if_exists (T::foo) {
        }
      }
      )test",

      R"test(
      template<typename T>
      void x(T &t) {
        __if_not_exists (T::foo) {
        }
      }
      )test"
  ));
}

TEST(ASTStructure, DeclStmt) {
  ASSERT_TRUE(compareStmt(
      "int y = 0;",
      "long v = 0;"
  ));
  ASSERT_FALSE(compareStmt(
      "int y = 0;",
      "int y = (1 + 1);"
  ));
  ASSERT_FALSE(compareStmt(
      "int a, b = 0;",
      "int b = 0;"
  ));
}

TEST(ASTStructure, ArraySubscriptExpr) {
  ASSERT_FALSE(compareStmt(
      "int i[2]; i[1] = 0;",
      "int i[2]; i[1 + 0] = 0;"
  ));
  ASSERT_FALSE(compareStmt(
      "int i[2]; (1)[i] = 0;",
      "int i[2]; (1 + 0)[i] = 0;"
  ));
  ASSERT_FALSE(compareStmt(
      "int i[2]; (i)[1 + 0] = 0;",
      "int i[2]; (1 + 0)[i] = 0;"
  ));
}

TEST(ASTStructure, ConditionalOperator) {
  ASSERT_TRUE(compareStmt(
      "int y = true ? 1 : 0;",
      "int x = false ? 0 : 1;"
  ));
  // first operand different
  ASSERT_FALSE(compareStmt(
      "int y = true == true ? 1 : 0;",
      "int y = false ? 1 : 0;"
  ));
  // second operand different
  ASSERT_FALSE(compareStmt(
      "int y = true ? 1 : 0;",
      "int y = true ? 1 + 1 : 0;"
  ));
  // third operand different
  ASSERT_FALSE(compareStmt(
      "int y = true ? 1 : 0;",
      "int y = true ? 1 : 0 + 0;"
  ));

  // Check GNU version
  ASSERT_FALSE(compareStmt(
      "int y = 1 ? : 0;",
      "int y = 1 ? : 0 + 0;"
  ));
  ASSERT_FALSE(compareStmt(
      "int y = 1 ? : 0;",
      "int y = 1 + 1 ? : 0;"
  ));
}

TEST(ASTStructure, CXXFold) {
  ASSERT_FALSE(compareStructure("x", "x",
      "template<class... A> bool x(A... args) { return (... && args); }",
      "template<class... A> bool x(A... args) { return (... || args); }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "template<class... A> bool x(A... args) { return (... && args); }",
      "template<class... A> bool x(A... args) { return (args && ...); }"
  ));
}

TEST(ASTStructure, CXXOperatorCallExpr) {
  ASSERT_FALSE(compareStructure("x", "x",
      R"test(
      class X {
      public:
        void operator-=(int i);
        void operator+=(int i);
      };
      void x() { X x; x += 1; }
      )test",

      R"test(
      class X {
      public:
        void operator-=(int i);
        void operator+=(int i);
      };
      void x() { X x; x -= 1; }
      )test"
  ));
}

TEST(ASTStructure, CXXTryStmt) {
  ASSERT_TRUE(compareStmt(
      "try { int x; } catch (int x) {}",
      "try { int y; } catch (int x) {}"
  ));
  ASSERT_FALSE(compareStmt(
      "try { int x; } catch (int x) {}",
      "try { } catch (int x) {}"
  ));
}

TEST(ASTStructure, DoStmt) {
  ASSERT_TRUE(compareStmt(
      "do { int x; } while (true);",
      "do { int y; } while (false);"
  ));
  ASSERT_FALSE(compareStmt(
      "do { int x; } while (true);",
      "do { } while (true);"
  ));
  ASSERT_FALSE(compareStmt(
      "int v; do { int x; } while ((v = 1));",
      "int v; do { int y; } while (true);"
  ));
}

TEST(ASTStructure, LambdaExpr) {
  ASSERT_TRUE(compareStmt(
      "auto a = [](){ return 1; };",
      "auto a = [](){ return 2; };"
  ));
  ASSERT_FALSE(compareStmt(
      "int i; auto a = [](){ return 2; };",
      "int i; auto a = [](){ return 1 + 1; };"
  ));
  ASSERT_FALSE(compareStmt(
      "int i; auto a = [i](){ return 1; };",
      "int i; auto a = [&i](){ return 1; };"
  ));
  ASSERT_FALSE(compareStmt(
      "auto a = [](int i){ return i; };",
      "auto a = [](int i, int b){ return i; };"
  ));
}

TEST(ASTStructure, CompoundStmt) {
  ASSERT_TRUE(compareStmt(
      "int x;",
      "int x;"
  ));
  ASSERT_FALSE(compareStmt(
      "int x;",
      "int x; int y;"
  ));
}

TEST(ASTStructure, Labels) {
  ASSERT_TRUE(compareStmt(
      "lbl: goto lbl;",
      "lbl: goto lbl;"
  ));
  ASSERT_FALSE(compareStmt(
      "lbl: goto lbl;",
      "lbl2: goto lbl2;"
  ));

  ASSERT_TRUE(compareStmt(
      "void* lbladdr = &&lbl; goto *lbladdr; lbl:;",
      "void* lbladdr = &&lbl; goto *lbladdr; lbl:;"
  ));
  ASSERT_FALSE(compareStmt(
      "lbl2:; void* lbladdr = &&lbl; goto *lbladdr; lbl:;",
      "lbl2:; void* lbladdr = &&lbl2; goto *lbladdr; lbl:;"
  ));
}

TEST(ASTStructure, WhileStmt) {
  ASSERT_TRUE(compareStmt(
      "while (true) { int x; }",
      "while (false) { int y; }"
  ));
  ASSERT_FALSE(compareStmt(
      "while (true) { int x; }",
      "while (false) { }"
  ));

  ASSERT_TRUE(compareStmt(
      "while (int v = 0) { int x; }",
      "while (int w = 0) { int y; }"
  ));
  ASSERT_FALSE(compareStmt(
      "int v; while ((v = 0)) { int x; }",
      "int v; while (false) { int y; }"
  ));
}

TEST(ASTStructure, NumberLiterals) {
  ASSERT_TRUE(compareStmt(
      "double x = 1;",
      "double x = 1l;"
  ));
  ASSERT_TRUE(compareStmt(
      "double x = 1u;",
      "double x = 1l;"
  ));
  ASSERT_TRUE(compareStmt(
      "double x = 1.0;",
      "long x = 1l;"
  ));
  ASSERT_TRUE(compareStmt(
      "double x = 1.0f;",
      "double x = 1l;"
  ));
}

TEST(ASTStructure, AtomicExpr) {
  ASSERT_TRUE(compareStmt(
      "int i[2]; __atomic_store_n(i, 1, __ATOMIC_RELAXED);",
      "int j[2]; __atomic_store_n(j, 1, __ATOMIC_RELAXED);"
  ));
  ASSERT_FALSE(compareStmt(
      "int i[2]; __atomic_store_n(i, 1, __ATOMIC_RELAXED);",
      "int i[2]; __atomic_store_n(i + 1, 1, __ATOMIC_RELAXED);"
  ));
  ASSERT_FALSE(compareStmt(
      "int i[2]; __atomic_store_n(i, 1, __ATOMIC_RELAXED);",
      "int i[2]; __atomic_store_n(i, 1 + 1, __ATOMIC_RELAXED);"
  ));
  ASSERT_FALSE(compareStmt(
      "int i[2]; __atomic_exchange_n(i, 1, __ATOMIC_RELAXED);",
      "int i[2]; __atomic_store_n   (i, 1, __ATOMIC_RELAXED);"
  ));
}

TEST(ASTStructure, BinaryOperator) {
  ASSERT_TRUE(compareStructure("x", "x",
      "int x() { return 1 + 4 * 8; }",
      "int x() { return 2 + 3 * 9; }"
  ));
  // Different operations
  ASSERT_FALSE(compareStructure("x", "x",
      "int x() { return 1 + 4 - 8; }",
      "int x() { return 2 + 3 * 9; }"
  ));
}

TEST(ASTStructure, UnaryOperator) {
  ASSERT_TRUE(compareStructure("x", "x",
      "int x() { return -8; }",
      "int x() { return -9; }"
  ));
  // Different operations
  ASSERT_FALSE(compareStructure("x", "x",
      "int x() { return -8; }",
      "int x() { return +8; }"
  ));
}

TEST(ASTStructure, InitListExpr) {
  ASSERT_TRUE(compareStructure("x", "x",
      "struct A {int a, b, c; }; void x() { A a = {1, 2, 3}; }",
      "struct A {int a, b, c; }; void x() { A a = {4, 5, 6}; }"
  ));
  // Different operations
  ASSERT_FALSE(compareStructure("x", "x",
     "struct A {int a, b, c; }; void x() { A a = {1 + 1, 2, 3}; }",
     "struct A {int a, b, c; }; void x() { A a = {2, 2, 3}; }"
 ));
}

TEST(ASTStructure, Casting) {
  ASSERT_TRUE(compareStructure("x", "x",
      "int x() { return static_cast<unsigned>(1); }",
      "int x() { return static_cast<long>(1); }"
  ));
  ASSERT_TRUE(compareStructure("x", "x",
      "int x() { return static_cast<unsigned>(1); }",
      "int x() { return static_cast<long>(1); }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "int i[2] = {0, 0}; int *x() { return static_cast<int *>(i); }",
      "const int i[2] = {0, 0}; int *x() { return const_cast<int *>(i); }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "int x() { return static_cast<unsigned>(1); }",
      "int x() { return static_cast<unsigned>(1 + 1); }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "int x() { return (int) (1 + 1); }",
      "int x() { return (int) (1); }"
  ));
}

TEST(ASTStructure, CXXCatchStmt) {
  ASSERT_TRUE(compareStmt(
      "try {} catch (long x) {}",
      "try {} catch (int x) {}"
  ));
  ASSERT_FALSE(compareStmt(
      "try {} catch (...) { int x; }",
      "try {} catch (...) {}"
  ));
  ASSERT_FALSE(compareStmt(
      "try {} catch (int x) {}",
      "try {} catch (...) {}"
  ));
}

TEST(ASTStructure, ForStmt) {
  ASSERT_TRUE(compareStructure("array_sum", "ArraySum",
      "int array_sum(int* array, unsigned len) {\n"
      "  int sum = 0;\n"
      "  for (unsigned i = 0; i < len; i++)\n"
      "    sum += array[i];\n"
      "  return sum;\n"
      "}\n",

      "int ArraySum(int* InputArray, unsigned Length) {\n"
      "  int Sum = 0;\n"
      "  for (unsigned j = 0; j < Length; j++)\n"
      "    Sum += InputArray[j];\n"
      "  return Sum;\n"
      "}\n"
  ));

  ASSERT_FALSE(compareStructure("array_sum", "array_sum",
      "int array_sum(int* array, unsigned len) {"
      "  int sum = 0;"
      "  for (unsigned i = 0; i < len; i++)"
      "    sum += array[i];"
      "  return sum;"
      "}",

      "int array_sum(int* array, unsigned len) {"
      "  int sum = 0;"
      "  for (unsigned i = 0; i < len; i++)"
      "    sum += array[i];"
      "  if (sum < 0) return 0;" // Note that we added an if here and so the
                                 // structure changed
      "  return sum;"
      "}"
  ));

  ASSERT_FALSE(compareStmt(
      "for (int i = 0; i < 100; i++) { i++; }",
      "for (int j = 0; j < 100; j++) { }"
  ));
}


// Macro tests

TEST(ASTStructure, MacroTest) {
  ASSERT_TRUE(isHashed(Stmt::StmtClass::DoStmtClass,
      R"test(
      #define GTEST1(Code) void foo() { Code }
      #define GTEST2(Code) GTEST1({ int gtest_var; Code })
      GTEST2({
        do {
          int i = 0;
          int j = 2;
        } while(0);
      })
      )test"
  ));
  ASSERT_FALSE(isHashed("foo",
      R"test(
      #define GTEST1(Code) void foo() { Code }
      #define GTEST2(Code) GTEST1({ while(false){} int gtest_var; Code })
      GTEST2({
        do {
          int i = 0;
          int j = 2;
        } while(0);
      })
      )test"
  ));
  ASSERT_FALSE(isHashed(Stmt::StmtClass::WhileStmtClass,
      R"test(
      #define GTEST1(Code) void foo() { Code }
      #define GTEST2(Code) GTEST1({ while(false){} Code })
      GTEST2({})
      )test"
  ));
}

// Imported tests from the related GSoC 2015 project

TEST(ASTStructure, GSoC2015CompoundStmt) {
  ASSERT_TRUE(compareStructure("x", "x",
      R"test(
      void x() {
        int a, b, c, d, e;
        a++;
        b--;
        c++;
        d--;
        e--;
        d*=2;
      }
      )test",

      R"test(
      void x() {
        int z, x, q, w, r;
        z++;
        x--;
        q++;
        w--;
        r--;
        w*=2;
      }
      )test"
  ));
}

TEST(ASTStructure, GSoC2015CompoundStmtLocals) {
  ASSERT_TRUE(compareStructure("x", "x",
      R"test(
      void x() {
        int one = 21,
            two = -1;
        if (one == 0) {
          two++;
          for (two = 0; two < 10; two++) {
            one--;
          }
          one = 21;
        }
      }
      )test",

      R"test(
      void x() {
        int a = 21,
            b = -1;
        if (a == 0) {
          b++;
          for (b = 0; b < 10; b++) {
            a--;
          }
          a = 21;
        }
      }
      )test"
  ));
}

TEST(ASTStructure, CompoundStmtLocal) {
  ASSERT_TRUE(compareStructure("x", "x",
      R"test(
      int global_one,
          global_two;

      void x() {
        global_one = 21;
        if (global_one == 0) {
          global_two++;
          for (global_two = 0; global_two < 10; global_two++) {
            global_one--;
          }
          global_one = 21;
        }
      }
      )test",

      R"test(
      int global_one,
          global_two;

      void x() {
        global_two = 21;
        if (global_two == 0) {
          global_one++;
          for (global_one = 0; global_one < 10; global_one++) {
            global_two--;
          }
          global_two = 21;
        }
      }
      )test"
  ));
}

// Use case tests

TEST(ASTStructure, ImageTest) {
  ASSERT_TRUE(compareStructure("testWidthRanges", "testHeightRanges",
    R"test(struct Image {
      int width() { return 0; }
      int height() { return 0; }
      void setWidth(int x) {}
      void setHeight(int y) {}
    };
    void assert(bool);
    void testWidthRanges() {
      Image img;
      img.setWidth(0);
      assert(img.width() == 0);
      img.setWidth(1);
      assert(img.width() == 1);
      try {
        img.setWidth(-1);
        assert(false);
      } catch (...) { }
    })test",

    R"test(struct Image {
      int width() { return 0; }
      int height() { return 0; }
      void setWidth(int x) {}
      void setHeight(int y) {}
    };
    void assert(bool);
    void testHeightRanges() {
      Image img;
      img.setHeight(0);
      assert(img.height() == 0);
      img.setHeight(1);
      assert(img.height() == 1);
      try {
        img.setWidth(-1);
        assert(false);
      } catch (...) { }
    })test"
  ));


  ASSERT_FALSE(compareStructure("testWidthRanges", "testHeightRanges",
    "struct Image {\n"
    "  int width() { return 0; }\n"
    "  int height() { return 0; }\n"
    "  void setWidth(int x) {}\n"
    "  void setHeight(int y) {}\n"
    "};"
    "void assert(bool);\n"
    "void testWidthRanges() {\n"
    "  Image img;\n"
    "  img.setWidth(0);\n"
    "  assert(img.width() == 0);\n"
    "  img.setWidth(1);\n"
    "  assert(img.width() == 1);\n"
    "  try {\n"
    "    img.setWidth(-1);\n"
    "    assert(false);\n"
    "  } catch (...) { }\n"
    "}\n",

    "struct Image {\n"
    "  int width() { return 0; }\n"
    "  int height() { return 0; }\n"
    "  void setWidth(int x) {}\n"
    "  void setHeight(int y) {}\n"
    "};"
    "void assert(bool);\n"
    "void testHeightRanges() {\n"
    "  Image img;\n"
    "  img.setHeight(0);\n"
    "  assert(img.height() == 0);\n"
    "  img.setHeight(1);\n" // Note that we are missing an assert here
                            // and so the structure changed
    "  try {\n"
    "    img.setWidth(-1);\n"
    "    assert(false);\n"
    "  } catch (...) { }\n"
    "}\n"
  ));
}
