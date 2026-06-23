__attribute__((constructor)) int cfoo() { return 0; }
__attribute__((destructor)) int dbar() { return 0; }
int val = 10;
int foo() { return val; }
int bar() { return foo(); }
