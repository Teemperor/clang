// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config unix.mallo:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config uni:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config uni.:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config ..:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config unix.:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config unrelated:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config unix.Malloc:Optimistic=true

// NO_ANALYZER: error: no analyzer checkers are associated with

// valid global config:
// RUN: %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config ipa=dynamic-bifurcate
// invalid global config:
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config ipaFunnyTypo=true 2>&1 | FileCheck --check-prefix=NO_CONFIG %s

// valid package config:
// RUN: %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config nullability:NoDiagnoseCallsToSystemHeaders=true
// package config in wrong package:
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config cplusplus:NoDiagnoseCallsToSystemHeaders=true 2>&1 | FileCheck --check-prefix=NO_CONFIG %s
// invalid package config:
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config unix:OptimisticWithTypoSuffix=true 2>&1 | FileCheck --check-prefix=NO_CONFIG %s

// valid checker config:
// RUN: %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config alpha.clone.CloneChecker:MinimumCloneComplexity=1
// invalid checker config:
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config alpha.clone.CloneChecker:MinimumCloneComplexityWithTypo=1 2>&1 | FileCheck --check-prefix=NO_CONFIG %s

// check that we don't do silly stuff on a trailing ':'
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config alpha.clone.CloneChecker:=1 2>&1 | FileCheck --check-prefix=NO_CONFIG %s

// NO_CONFIG: error: no analyzer configs are associated with

// Just to test clang is working.
# foo
