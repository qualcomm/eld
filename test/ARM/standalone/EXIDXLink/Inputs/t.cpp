class A {
public:
  int foo() {
    bar = 2;
    try {
      throw 1;
    }
    catch (...) {
    return 1;
    }
    return 0;
  }
public:
  static int bar;
};


int A::bar = 0;

int main() { A a; return a.foo(); }

