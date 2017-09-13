template <long N, long M1, long M2, long M3> struct Factorial
{
    enum { val = Factorial<N-1, M1, M2, M3 + 1>::val };
    enum { val2 = Factorial<N-1, M2, M1 + 1, M3>::val };
    enum { val3 = Factorial<N-1, M3 + 1, M2, M1>::val };
    int foo1(int a = 1, int b = N, int c = M1) { return 1; }
    int foo2(int a = 1, int b = N, int c = M2) { return 2; }
    int foo3(int a = 1, int b = N, int c = M3) { return 3; }
};
template<long m1, long m2, long m3>
struct Factorial<0, m1, m2, m3>
{
   enum { val = 0 };
};
int main()
{
    return Factorial<60, 1000, 2000, 3000>::val;
}

