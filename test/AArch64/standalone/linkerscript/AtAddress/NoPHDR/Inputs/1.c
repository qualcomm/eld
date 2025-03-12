__attribute__((section(".foo@0x100"))) int foo() { return 0; }
__attribute__((section(".bar@0x200"))) int bar() { return 0; }
__attribute__((section(".baz@0x300"))) int baz() { return 0; }
__attribute__((section(".main@0"))) int main() { return 0; }
