{
  global:
    extern "C++" {
      "ns::MyClass::foo()";
      "ns::MyClass::staticMethod(double)";
      "ns::freeFuncChar*";
      globalFunc*;
    };
  local:
    *;
};
