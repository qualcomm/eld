extern int __init_array_end;
__attribute__((section(".init_array"))) int foo() { return 0; }
int *blah = &__init_array_end;
