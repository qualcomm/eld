__thread int b = 0;
__thread int c = 0;

int bar() {
  b = 1;
  c = 2;
  return b + c;
}

int main() { return bar(); }
