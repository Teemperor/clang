// RUN: %clang_cc1 -analyze -std=c++11 -analyzer-checker=clone.CloneReporter -verify %s

#define GTEST_FOO(TESTNAME, CODE) int test##TESTNAME () { CODE ; return 0; }

struct File {
  bool IsClosed();
  bool IsBuffered();
  void Flush();
  void Close();
  void WriteChar(char c);
};

#define CLOSE_FILE(F)        \
  {                          \
    if (!F.IsClosed()) {     \
      if (F.IsBuffered())    \
        F.Flush();           \
      F.Close();             \
    }                        \
  }                          \

void Write(char c) {
  File F;
  F.WriteChar(c);
  CLOSE_FILE(F);
}

void WriteTwice(char c) {
  File F;
  F.WriteChar(c);
  F.WriteChar(c);
  CLOSE_FILE(F);
}

void WriteNoMacro(char c) {
  File F;
  F.WriteChar(c); // expected-note{{Related code clone is here.}}
  {
    if (!F.IsClosed()) {
      if (F.IsBuffered())
        F.Flush();
      F.Close();
    }
  }
}

void WriteTwiceNoMacro(char c) {
  File F;
  F.WriteChar(c);
  F.WriteChar(c); // expected-warning{{Detected code clone.}}
  {
    if (!F.IsClosed()) {
      if (F.IsBuffered())
        F.Flush();
      F.Close();
    }
  }
}
