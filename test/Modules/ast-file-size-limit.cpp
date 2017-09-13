// RUN: %clang_cc1 -xc++ -std=c++11 -emit-pch -o %t.ast %S/Inputs/ast-file-size-limit.h 2>&1 | FileCheck -check-prefix=CHECK-TOO-BIG %s
// CHECK-TOO-BIG: error: AST file bigger than AST file size limit. File is
