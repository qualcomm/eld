int x;
int y __attribute__((section("mydata")));
int main()  __attribute__((section("mycode_1")));
void foo() __attribute__((section("mycode_2")));
extern void bar();
extern void bar2();

int main() {
  x = 123;
  return 0;
}

void foo() {
  y = 456;
  bar();
  bar2();
}
