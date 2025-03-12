int a = 10;
int foo() {
  return a;
}

int bar() {
  return a;
}

int main() {
  return foo() + bar();
}
