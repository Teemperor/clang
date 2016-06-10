// RUN: %clang_cc1 -analyze -std=c++11 -analyzer-checker=clone.CopyPasteChecker -verify %s

int a = 0;

void foo() {
  int b = 0;
  if (a > 0) {
    a++;
    a++;
    b++;
    a++;
    b--;
  }
}

void boo() {
  int c = 0;
  if (a > 0) {
    a++;
    a++;
    a++;
    a++;
    c--;
  }
}

