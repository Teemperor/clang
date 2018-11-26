// RUN: %clang_analyze_cc1 -std=c++11 -analyzer-checker=alpha.clone.CloneChecker -analyzer-config alpha.clone.CloneChecker:MinimumCloneComplexity=5 -verify %s

void log(int i);

void foo0(int a, int b) { // expected-warning{{Duplicate code detected}}
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
}

void foo1() { // expected-note{{Similar code here}}
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
  log(1);
}
