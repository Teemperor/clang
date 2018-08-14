// RUN: clang-import-test -dump-ast -import %S/Inputs/S.cpp -expression %s | FileCheck %s
// CHECK: CXXScalarValueInitExpr

void expr() {
  int i = si();
  float f = sf();
}
