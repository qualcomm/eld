__attribute__((section(".foo@0x99"))) int foo() { return 0; }
__attribute__((section(".bar@0x199"))) int bar() { return 0; }
__attribute__((section(".baz@0x299"))) int baz() { return 0; }
__attribute__((section(".main@0"))) int main() { return 0; }
