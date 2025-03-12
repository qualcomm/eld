template <class T> class TestClass {
public:
  static T bar() { return 1; }
};

int bar() { return TestClass<int>::bar(); }
