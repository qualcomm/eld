int foo() { return 0; }
int bar() { return 0; }
__attribute__((aligned(4))) __attribute__((section("baz@0"))) int baz() { return foo(); }
extern int __start_text;
extern int __stop_text;
int *start = &__start_text;
int *stop = &__stop_text;
int main() { return 0; }
