template <class T> class TestClass {
public:
  static T bar() noexcept { return 1; }
};

int bar() noexcept { return TestClass<int>::bar(); }
