// RUN: %clang_cc1 -fmodules-cache-path=%t/cache -fmodules -fimplicit-module-maps -emit-obj -I%S/Inputs/delay-emit-check/ %s -o %t.o

namespace a { namespace b {
  int foo();
}}
#include "Module.h"
