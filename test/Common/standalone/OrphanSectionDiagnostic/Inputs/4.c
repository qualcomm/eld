int foo() { return 1; }
int bar() { return 2; }
__attribute__((section(".bss.str"))) char str[0];