// RUN: %clang_cc1 -analyze -std=c++11 -analyzer-checker=clone.CopyPasteChecker -verify %s

#define GTEST_FOO(TESTNAME, CODE) void test##TESTNAME () { CODE }


struct Image {
  int w, h;
  int width() { return w; }
  int height() { return h; }
  void setWidth(int x) { w = x; }
  void setHeight(int y) { h = y; }
};
void assert(bool) {}

GTEST_FOO(WidthRanges, { // expected-note{{Suggestion is based on this similar algorithm.}}
  Image img;
  img.setWidth(5);
  assert(img.width() == 5);
  img.setWidth(1);
  assert(img.width() == 1);
  img.setWidth(0);
  assert(img.width() == 0);
})

GTEST_FOO(HeightRanges, { // expected-warning{{Maybe you wanted to use 'Image::setHeight' instead of 'Image::setWidth'?}}
  Image img;
  img.setHeight(5);
  assert(img.height() == 5);
  img.setHeight(1);
  assert(img.height() == 1);
  img.setWidth(0);
  assert(img.height() == 0);
})
