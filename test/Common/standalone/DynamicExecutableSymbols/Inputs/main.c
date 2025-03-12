extern char common_a;
extern char data_a;
int foo();

int main() {
  return foo() + common_a + data_a;
}
