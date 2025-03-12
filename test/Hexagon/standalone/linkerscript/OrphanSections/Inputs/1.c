__attribute__((section(".text.foo"))) int foo() { return 0; }
__attribute__((section(".text.main"))) int main() { return 0; }
__attribute__((section(".text.bar"))) int bar() { return 0; }

__attribute__((section(".data.foo"))) int dfoo  = 10;
__attribute__((section(".data.main"))) int dmain = 20;
__attribute__((section(".data.bar"))) int dbar = 30;

__attribute__((section(".bss.foo"))) int bfoo  = 0;
__attribute__((section(".bss.main"))) int bmain = 0;
__attribute__((section(".bss.bar"))) int bbar = 0;
