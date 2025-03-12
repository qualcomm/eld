//__attribute__((weak)) extern int __start_bar;
int __start_bar;
__attribute__((section("bar"))) int foo() { return 0; }
int main() {
return &__start_bar;
}
