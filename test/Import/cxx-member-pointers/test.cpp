// RUN: clang-import-test -dump-ast -import %S/Inputs/S.cpp -expression %s | FileCheck %s

// FIXME: 'int S::*' should probably be imported as 'int struct S::*'.

// CHECK: VarDecl
// CHECK-SAME: int S::*
// CHECK-NEXT: CallExpr
// CHECK-NEXT: ImplicitCastExpr
// CHECK-SAME: int S::*(*)()
// CHECK-NEXT: DeclRefExpr
// CHECK-SAME: int S::*()

void expr() {
  int S::*p = iptr();
  S s;
  s.i = 3;
  int i = s.*p;
}
