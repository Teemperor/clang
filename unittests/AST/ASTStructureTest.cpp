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

bool compareStructure(const std::string& DeclNameA,
                      const std::string& DeclNameB,
                      const std::string& codeA,
                      const std::string& codeB) {
  auto ASTUnitA = tooling::buildASTFromCode(codeA);
  auto ASTUnitB = tooling::buildASTFromCode(codeB);

  auto HashA = ASTStructure(ASTUnitA->getASTContext());
  auto HashB = ASTStructure(ASTUnitB->getASTContext());

  // Iterate over all top level declarations and search
  // the specified one in the first translation unit
  for (auto DA : ASTUnitA->getASTContext().getTranslationUnitDecl()->decls()) {
      if (auto NamedDA = dyn_cast<NamedDecl>(DA)) {
        if (NamedDA->getNameAsString() == DeclNameA) {

          // Iterate over all top level declarations and search
          // the specified one in the second translation unit
          for (auto DB : ASTUnitB->getASTContext().getTranslationUnitDecl()->decls()) {
            if (auto NamedDB = dyn_cast<NamedDecl>(DB)) {
              if (NamedDB->getNameAsString() == DeclNameB) {
                // If we have both declarations, compare
                // their hashes
                return HashA.getHash(DA) == HashB.getHash(DB);
              }
            }
          }
        }
      }
  }
  // We couldn't find the two specified declarations, so we abort
  // fail the whole test.
  assert(false);
}


TEST(ASTStructure, InheritMembers) {
  ASSERT_TRUE(compareStructure("D1", "D2",
      "class B1 { int x1; int y1; };\n"
      "class D1 : public B1 { };",

      "class B2 { int x2; int y2; };\n"
      "class D2 : public B2 { };"
  ));
  ASSERT_FALSE(compareStructure("D1", "D2",
      "class B1 { int x2; };\n"
      "class D1 : public B1 { };",

      "class B2 { int x1; int y1; };\n"
      "class D2 : public B2 { };"
  ));
}

TEST(ASTStructure, InheritFunctions) {
  ASSERT_TRUE(compareStructure("D1", "D2",
      "class B1 { void x() { int j; } };\n"
      "class D1 : public B1 { };",

      "class B2 { void y() { int j; } };\n"
      "class D2 : public B2 { };"
  ));
  ASSERT_FALSE(compareStructure("D1", "D2",
      "class B1 { void x() { int j; } };\n"
      "class D1 : public B1 { };\n",

      "class B2 { void y() { } };\n"
      "class D2 : public B2 { };"
  ));
}

TEST(ASTStructure, VariadicFunctions) {
  ASSERT_FALSE(compareStructure("x", "x",
      "void x(...) { }",
      "void x() { }"
  ));
}

TEST(ASTStructure, StaticFunctions) {
  ASSERT_FALSE(compareStructure("B", "B",
      "class B { void x() { } };",
      "class B { static void x() { } };"
  ));
}

TEST(ASTStructure, ConstFunctions) {
  ASSERT_FALSE(compareStructure("B", "B",
      "class B { void x() const { } };",
      "class B { void x() { } };"
  ));
}

TEST(ASTStructure, VolatileFunctions) {
  ASSERT_FALSE(compareStructure("B", "B",
      "class B { void x() volatile { } };",
      "class B { void x() { } };"
  ));
}

TEST(ASTStructure, VirtualFunctions) {
  ASSERT_FALSE(compareStructure("B", "B",
      "class B { void x() { } };",
      "class B { virtual void x() { } };"
  ));
}

TEST(ASTStructure, IfStmt) {
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { if (true) {} }",
      "void x() { if (false) {} }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { if (true) { int x; } }",
      "void x() { if (false) {} }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { if (int y = 0) {} }",
      "void x() { if (false) {} }"
  ));
}

TEST(ASTStructure, DeclStmt) {
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int y = 0; }",
      "void x() { int y = (1 + 1); }"
  ));
}

TEST(ASTStructure, ArraySubscriptExpr) {
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int i[2]; i[1] = 0; }",
      "void x() { int i[2]; i[1 + 0] = 0; }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int i[2]; (1)[i] = 0; }",
      "void x() { int i[2]; (1 + 0)[i] = 0; }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int i[2]; (i)[1 + 0] = 0; }",
      "void x() { int i[2]; (1 + 0)[i] = 0; }"
  ));
}

TEST(ASTStructure, ConditionalOperator) {
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { int y = true ? 1 : 0; }",
      "void x() { int x = false ? 0 : 1; }"
  ));
  // first operand different
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int y = true == true ? 1 : 0; }",
      "void x() { int y = false ? 1 : 0; }"
  ));
  // second operand different
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int y = true ? 1 : 0; }",
      "void x() { int y = true ? 1 + 1 : 0; }"
  ));
  // third operand different
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int y = true ? 1 : 0; }",
      "void x() { int y = true ? 1 : 0 + 0; }"
  ));

  // Check GNU version
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int y = 1 ? : 0; }",
      "void x() { int y = 1 ? : 0 + 0; }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int y = 1 ? : 0; }",
      "void x() { int y = 1 + 1 ? : 0; }"
  ));
}

TEST(ASTStructure, CXXTryStmt) {
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { try { int x; } catch (int x) {} }",
      "void x() { try { int y; } catch (int x) {} }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { try { int x; } catch (int x) {} }",
      "void x() { try { } catch (int x) {} }"
  ));
}

TEST(ASTStructure, DoStmt) {
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { do { int x; } while (true); }",
      "void x() { do { int y; } while (false); }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { do { int x; } while (true); }",
      "void x() { do { } while (true); }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int v; do { int x; } while ((v = 1)); }",
      "void x() { int v; do { int y; } while (true); }"
  ));
}

TEST(ASTStructure, CompoundStmt) {
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { int x; }",
      "void x() { int x; }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int x; }",
      "void x() { int x; int y; }"
  ));
}

TEST(ASTStructure, WhileStmt) {
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { while (true) { int x; } }",
      "void x() { while (false) { int y; } }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { while (true) { int x; } }",
      "void x() { while (false) { } }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int v; while ((v = 0)) { int x; } }",
      "void x() { int v; while (false) { int y; } }"
  ));
}

TEST(ASTStructure, NumberLiterals) {
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { double x = 1; }",
      "void x() { double x = 1l; }"
  ));
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { double x = 1u; }",
      "void x() { double x = 1l; }"
  ));
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { double x = 1.0; }",
      "void x() { long x = 1l; }"
  ));
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { double x = 1.0f; }",
      "void x() { double x = 1l; }"
  ));
}

TEST(ASTStructure, AtomicExpr) {
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { int i[2]; __atomic_store_n(i, 1, __ATOMIC_RELAXED); }",
      "void x() { int j[2]; __atomic_store_n(j, 1, __ATOMIC_RELAXED); }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int i[2]; __atomic_store_n(i, 1, __ATOMIC_RELAXED); }",
      "void x() { int i[2]; __atomic_store_n(i + 1, 1, __ATOMIC_RELAXED); }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int i[2]; __atomic_store_n(i, 1, __ATOMIC_RELAXED); }",
      "void x() { int i[2]; __atomic_store_n(i, 1 + 1, __ATOMIC_RELAXED); }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { int i[2]; __atomic_exchange_n(i, 1, __ATOMIC_RELAXED); }",
      "void x() { int i[2]; __atomic_store_n   (i, 1, __ATOMIC_RELAXED); }"
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
      "int *x() { int i[2]; return static_cast<int *>(i); }",
      "int *x() { const int i[2]; return const_cast<int *>(i); }"
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
  ASSERT_TRUE(compareStructure("x", "x",
      "void x() { try {} catch (long x) {} }",
      "void x() { try {} catch (int x) {} }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { try {} catch (...) { int x; } }",
      "void x() { try {} catch (...) {} }"
  ));
  ASSERT_FALSE(compareStructure("x", "x",
      "void x() { try {} catch (int x) {} }",
      "void x() { try {} catch (...) {} }"
  ));
}

TEST(ASTStructure, FunctionTemplates) {
  ASSERT_FALSE(compareStructure("x", "x",
      "template <class T> void x() { }",
      "template <class T, class T2> void x() { }"
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

  ASSERT_FALSE(compareStructure("A", "A",
      "void A() { for (int i = 0; i < 100; i++) { i++; } }",

      "void A() { for (int j = 0; j < 100; j++) { } }"
  ));
}

// Tests from GSoC 2015

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
    "struct Image {\n"
    "  int width() { return 0; }\n"
    "  int height() { return 0; }\n"
    "  void setWidth(int x) {}\n"
    "  void setHeight(int y) {}\n"
    "};"
    "void assert(bool);\n"
    "void testWidthRanges() {"
    "  Image img;"
    "  img.setWidth(0);"
    "  assert(img.width() == 0);"
    "  img.setWidth(1);"
    "  assert(img.width() == 1);"
    "  try { "
    "    img.setWidth(-1);"
    "    assert(false); "
    "  } catch (...) { }"
    "}",

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
    "  img.setHeight(1);\n"
    "  assert(img.height() == 1);\n"
    "  try {\n"
    "    img.setWidth(-1);\n"
    "    assert(false);\n"
    "  } catch (...) { }\n"
    "}\n"
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
