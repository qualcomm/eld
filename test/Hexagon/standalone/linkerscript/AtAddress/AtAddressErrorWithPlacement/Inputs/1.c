__attribute__((section(".foo"))) int foo() { return 0; }
__attribute__((section(".bar"))) int bar() { return 0; }
__attribute__((section(".baz@0x350"))) int baz() { return 0; }
__attribute__((section(".main@0x300"))) int main() { return 0; }
