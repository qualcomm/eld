__attribute__((section("QSR_STR.fmt.rodata.1"))) const int val = 100;
int myval = 0;
int main() {
  return val;
}

int foo() { return 0; }
