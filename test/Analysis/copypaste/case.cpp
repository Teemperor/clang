// RUN: %clang_analyze_cc1 -std=c++11 -analyzer-checker=alpha.clone.CloneChecker -analyzer-config alpha.clone.CloneChecker:MinimumCloneComplexity=5 -verify %s

void log(int i);

void foo(int a) {
  switch(a) {
  case 0:
  { // expected-warning{{Duplicate code detected}}
    log(1);
    log(1);
    log(1);
    log(1);
  }
  case 1:
  { // expected-note{{Similar code here}}
    log(1);
    log(1);
    log(1);
    log(1);
  }
  }
}

void foo(int i);

void bar(int a) {
  switch(a) {
  case 0:
    foo(1);
    foo(1); // expected-warning{{Duplicate code detected}}
    foo(1);
    foo(1);
    foo(1);
  case 1:
    foo(1);
    foo(1); // expected-note{{Similar code here}}
    foo(1);
    foo(1);
    foo(1);
  case 2:
    foo(1);
    foo(1); // expected-note{{Similar code here}}
    foo(1);
    foo(1);
    foo(1);
  case 3:
    foo(1);
    foo(1); // expected-note{{Similar code here}}
    foo(1);
    foo(1);
    foo(1);
  }
}
