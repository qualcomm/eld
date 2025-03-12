extern int __start_foo;
extern int __stop_bar;

__attribute__((section("foo"))) int foo_fn (){
  return 1;
}

__attribute__((section("bar"))) int bar_fn (){
  return 2;
}

int main () {
  return __start_foo + __stop_bar;
}
