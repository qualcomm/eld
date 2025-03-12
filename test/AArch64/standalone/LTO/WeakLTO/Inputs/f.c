__attribute__((weak)) int weak_func();
int bar() { return weak_func(); }
