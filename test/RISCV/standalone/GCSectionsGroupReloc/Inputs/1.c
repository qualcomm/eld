int common;
int foo() { return 1; }

int bar()
{ common = 1;
  return foo() + common; }

int main() {
  return foo() ;
}
