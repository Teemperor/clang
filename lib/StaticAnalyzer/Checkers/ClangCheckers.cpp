//===--- ClangCheckers.h - Provides builtin checkers ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/ClangCheckers.h"
#include "clang/StaticAnalyzer/Core/CheckerRegistry.h"

// FIXME: This is only necessary as long as there are checker registration
// functions that do additional work besides mgr.registerChecker<CLASS>().
// The only checkers that currently do this are:
// - NSAutoreleasePoolChecker
// - NSErrorChecker
// - ObjCAtSyncChecker
// It's probably worth including this information in Checkers.td to minimize
// boilerplate code.
#include "ClangSACheckers.h"

using namespace clang;
using namespace ento;

void ento::registerBuiltinCheckers(CheckerRegistry &registry) {
#define GET_CHECKERS
#define CHECKER(FULLNAME,CLASS,DESCFILE,HELPTEXT,GROUPINDEX,HIDDEN)    \
  registry.addChecker(register##CLASS, FULLNAME, HELPTEXT);
#include "clang/StaticAnalyzer/Checkers/Checkers.inc"
#undef GET_CHECKERS

#define GET_GLOBAL_CONFIGS
#define GLOBAL_CONFIG(NAME) registry.addConfig(NAME);
#include "clang/StaticAnalyzer/Checkers/Checkers.inc"
#undef GET_GLOBAL_CONFIGS

#define GET_CHECKER_CONFIGS
#define CHECKER_CONFIG(CHECKER, NAME) registry.addConfig(CHECKER ":" NAME);
#include "clang/StaticAnalyzer/Checkers/Checkers.inc"
#undef GET_CHECKER_CONFIGS

#define GET_PACKAGE_CONFIGS
#define PACKAGE_CONFIG(PACKAGE, NAME) registry.addConfig(PACKAGE ":" NAME);
#include "clang/StaticAnalyzer/Checkers/Checkers.inc"
#undef GET_PACKAGE_CONFIGS
}
