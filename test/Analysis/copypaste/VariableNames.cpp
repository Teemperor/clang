// RUN: %clang_cc1 -analyze -std=c++11 -analyzer-checker=clone.CopyPasteChecker -verify %s

struct mutex {
  bool try_lock() { return false; }
  void unlock() {}
};

mutex m1;
int m1_global;

mutex m2;
int m2_global;

int read1() {
  int timer = 0;
  while(true) {
    if (m1.try_lock()) {
      int result = m1_global;
      m1.unlock(); // expected-note{{Other possibly faulty code clone instance is here.}}
      return result;
    } else {
      // busy wait
      timer++;
    }
  }
}


int read2() {
  int timer = 0;
  while(true) {
    if (m2.try_lock()) {
      int result = m2_global;
      m1.unlock(); // expected-warning{{Possibly faulty code clone. Maybe you wanted to use 'm2' instead of 'm1'?}}
      return result;
    } else {
      // busy wait
      timer++;
    }
  }
}
