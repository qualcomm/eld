extern __attribute__((weak)) int __start_bar;
extern __attribute__((weak)) int __stop_bar ;
__attribute__((section("bar"))) int foo() {
  return (int)(&__start_bar) + (int)(&__stop_bar);
}

int main() {
  return foo();
}
