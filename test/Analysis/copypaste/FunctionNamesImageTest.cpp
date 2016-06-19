// RUN: %clang_cc1 -analyze -std=c++11 -analyzer-checker=clone.CopyPasteChecker -verify %s

struct Image {
  int w, h;
  int width() { return w; }
  int height() { return h; }
  void setWidth(int x) { w = x; }
  void setHeight(int y) { h = y; }
};
void assert(bool) {}

void testWidthRanges() {
  Image img;
  img.setWidth(5);
  assert(img.width() == 5);
  img.setWidth(1);
  assert(img.width() == 1);
  img.setWidth(0); // expected-note{{Other possibly faulty code clone instance is here.}}
  assert(img.width() == 0);
}

void testHeightRanges() {
  Image img;
  img.setHeight(5);
  assert(img.height() == 5);
  img.setHeight(1);
  assert(img.height() == 1);
  img.setWidth(0); // expected-warning{{Possibly faulty code clone. Maybe you wanted to use 'Image::setHeight' instead of 'Image::setWidth'?}}
  assert(img.height() == 0);
}
