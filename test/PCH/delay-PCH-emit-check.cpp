// RUN: %clang_cc1 -x c++ -std=c++14 -fmodules -emit-module -fmodule-name=M %S/Inputs/empty-def-fwd-struct.modulemap -o %t.pcm
// RUN: %clang_cc1 -x c++ -std=c++14 -fmodules -emit-llvm -fmodule-file=%t.pcm %s -o %t.o

// expected-no-diagnostics

#include "empty-def-fwd-struct.h"
