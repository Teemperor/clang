// RUN: %clang_cc1 -analyze -std=c++11 -analyzer-checker=clone.CloneReporter -verify %s

struct mutex {
  bool try_lock() { return false; }
  void unlock() {}
};

extern mutex mutex1;
extern int mutex1_global;

extern mutex mutex2;
extern int mutex2_global;

extern int var2_accesses;

int read1() { // expected-note{{Related code clone is here.}}
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

int read2() { // expected-warning{{Detected code clone.}}
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
