// RUN: %clang_cc1  -fsyntax-only -verify -std=c++11 %s
// Don't crash due to the 'c :' as the return type of d.

template <typename a>
int b() {}
template <typename c>
c : d() { // expected-error {{unexpected ':'}} \
                                // expected-error {{nested name specifier 'c::' for declaration does not refer}}
  c &e = b<c>;
  auto i = e;
}
