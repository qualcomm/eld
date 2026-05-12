int x;
int y __attribute__((section("mydata")));
int main() __attribute__((section("mycode_1")));
void foo() __attribute__((section("mycode_2")));
extern void bar(void);
extern void bar2(void);

int main(void) {
  x = 123;
  return 0;
}

void foo(void) {
  y = 456;
  bar();
  bar2();
}
