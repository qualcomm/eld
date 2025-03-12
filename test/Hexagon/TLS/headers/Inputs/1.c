__thread double a = 2.0;
__thread int b;
int c;
int d = 1;
int main () {
  b = 2;
  c = 3;

  return (int)a + b + c + d + foo();
}

__attribute__((section(".foo"))) int foo() {
  return 1;
}
