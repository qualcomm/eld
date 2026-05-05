int foo();
extern int a;
int main() {
  return a + foo();
}
