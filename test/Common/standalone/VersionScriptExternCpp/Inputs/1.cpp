// C++ source file with various symbol types for testing extern "C++" in version
// scripts. The mangled names will be matched against demangled patterns in the
// version script.

namespace ns {

class MyClass {
public:
  // Demangled: ns::MyClass::foo()
  int foo() { return 1; }

  // Demangled: ns::MyClass::bar(int)
  int bar(int x) { return x + 1; }

  // Demangled: ns::MyClass::baz(int, float)
  int baz(int x, float y) { return x + y; }

  // Demangled: ns::MyClass::staticMethod(double)
  static int staticMethod(double x) { return 42; }
};

// Demangled: ns::freeFunc()
int freeFunc() { return 100; }

// Demangled: ns::freeFuncChar(char)
int freeFuncChar(char x) { return (int)x * 2; }

} // namespace ns

// Demangled: globalCppFunc()
int globalFunc() { return 200; }

// Entry point
extern "C" int _start() {
  ns::MyClass obj;
  return obj.foo() + obj.bar(1) + obj.baz(1, 2) +
         ns::MyClass::staticMethod(1.0) + ns::freeFunc() +
         ns::freeFuncChar('a') + globalFunc();
}
