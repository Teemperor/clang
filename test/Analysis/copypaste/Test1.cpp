// RUN: %clang_cc1 -analyze -std=c++11 -analyzer-checker=clone.CopyPasteChecker -verify %s

struct Image {
  int width() { return 0; }
  int height() { return 0; }
  void setWidth(int x) {}
  void setHeight(int y) {}
};
void assert(bool) {}

void testWidthRanges() {
  Image img;
  img.setWidth(0);
  assert(img.width() == 0);
  img.setWidth(1);
  assert(img.width() == 1);
  img.setWidth(-1);
  assert(false);
}

void testHeightRanges() {
  Image img;
  img.setHeight(0);
  assert(img.height() == 0);
  img.setHeight(1);
  assert(img.height() == 1);
  img.setWidth(-1);
  assert(false);
}
