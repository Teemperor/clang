
struct RAII {
  RAII();
  RAII(void (^b)());
  ~RAII();
};

void f() {
  RAII(^(void){;});
}
