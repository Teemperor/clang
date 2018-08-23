namespace {
void func1() {
}
}

namespace test_namespace1 { namespace {
 void func2() {}
}}

namespace test_namespace2 { namespace { namespace test_namespace3 {
 void func3() {}
}}}

namespace { namespace {
void func4() {
}
}}
