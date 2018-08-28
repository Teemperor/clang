// RUN: %clang_cc1 -std=c++11 -isystem %S/Inputs/clang -internal-externc-isystem %S/Inputs/libc -fexceptions -fcxx-exceptions -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++11 -isystem %S/Inputs/clang -internal-externc-isystem %S/Inputs/libc -fexceptions -fcxx-exceptions -fsyntax-only -verify -DREVERSE %s

// One of the headers is in a user include, so our redeclaration should fail.
// RUN: %clang_cc1 -std=c++11 -I %S/Inputs/clang -internal-externc-isystem %S/Inputs/libc -fexceptions -fcxx-exceptions -fsyntax-only -verify %s
// RUN: not %clang_cc1 -std=c++11 -isystem %S/Inputs/clang -I %S/Inputs/libc -fexceptions -fcxx-exceptions -fsyntax-only -verify -DREVERSE %s

// The same test cases again with enabled modules.
// The modules cases *all* pass because we marked both as [system].
// RUN: rm -rf %t
// RUN: %clang_cc1 -fmodules -fmodules-local-submodule-visibility -fimplicit-module-maps -fmodules-cache-path=%t \
// RUN:            -std=c++11 -isystem %S/Inputs/clang -internal-externc-isystem %S/Inputs/libc -fexceptions -fcxx-exceptions -fsyntax-only -verify %s
// RUN: rm -rf %t
// RUN: %clang_cc1 -fmodules -fmodules-local-submodule-visibility -fimplicit-module-maps -fmodules-cache-path=%t \
// RUN:            -std=c++11 -isystem %S/Inputs/clang -internal-externc-isystem %S/Inputs/libc -fexceptions -fcxx-exceptions -fsyntax-only -verify -DREVERSE %s
// RUN: rm -rf %t
// RUN: %clang_cc1 -fmodules -fmodules-local-submodule-visibility -fimplicit-module-maps -fmodules-cache-path=%t \
// RUN:            -std=c++11 -I %S/Inputs/clang -internal-externc-isystem %S/Inputs/libc -fexceptions -fcxx-exceptions -fsyntax-only -verify %s
// RUN: rm -rf %t
// RUN: %clang_cc1 -fmodules -fmodules-local-submodule-visibility -fimplicit-module-maps -fmodules-cache-path=%t \
// RUN:            -std=c++11 -isystem %S/Inputs/clang -I %S/Inputs/libc -fexceptions -fcxx-exceptions -fsyntax-only -verify -DREVERSE %s

// expected-no-diagnostics
#ifdef REVERSE
#include "mm_malloc.h"
#include "stdlib.h"
#else
#include "mm_malloc.h"
#include "stdlib.h"
#endif

void f() {
  free(nullptr);
}
