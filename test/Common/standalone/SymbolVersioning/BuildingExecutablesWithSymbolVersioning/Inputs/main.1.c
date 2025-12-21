int foo();

__asm__(".symver bar, foo@V1");
int bar();

int main() {
  return foo() + bar();
}