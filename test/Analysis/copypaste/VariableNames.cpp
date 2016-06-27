// RUN: %clang_cc1 -analyze -std=c++11 -analyzer-checker=clone.CopyPasteChecker -verify %s

struct mutex {
  bool try_lock() { return false; }
  void unlock() {}
};

extern mutex mutex1;
extern int mutex1_global;

extern mutex mutex2;
extern int mutex2_global;

extern int var2_accesses;

int read1() {
  unsigned timer = 0;
  while(true) {
    if (mutex1.try_lock()) {
      int result = mutex1_global;
      mutex1.unlock();
      return result;
    } else {
      // busy wait
      timer++;
    }
  }
}

int read2() {
  var2_accesses++;
  unsigned timer = 0;
  while(true) {
    if (mutex2.try_lock()) {
      int result = mutex2_global;
      mutex1.unlock(); // expected-warning{{Maybe you wanted to use 'mutex2' instead of 'mutex1'?}}
      return result;
    } else {
      // busy wait
      timer++;
    }
  }
}

int read3() {
  unsigned timer = 0;
  while(true) {
    if (mutex1.try_lock()) {
      int result = mutex1_global;
      mutex1.unlock(); // expected-note{{Suggestion is based on this similar algorithm.}}
      return result;
    } else {
      // busy wait
      timer++;
    }
  }
}
