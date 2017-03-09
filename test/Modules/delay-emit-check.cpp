// RUN: %clang_cc1 -fmodules-cache-path=%t/cache -fmodules -fimplicit-module-maps -I%S/Inputs/delay-emit-check/ %s -verify

// expected-no-diagnostics

namespace a { namespace b {
  int foo();
}}
#include "Module.h"
