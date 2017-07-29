// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config unix.mallo:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config uni:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config uni.:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config ..:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config unix.:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: not %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config unrelated:Optimistic=true 2>&1 | FileCheck --check-prefix=NO_ANALYZER %s
// RUN: %clang_analyze_cc1 -analyzer-checker=core,unix.Malloc -analyzer-config unix.Malloc:Optimistic=true

// Just to test clang is working.
# foo

// NO_ANALYZER: error: no analyzer checkers are associated with 
