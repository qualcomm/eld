void __attribute__((patchable)) f();

void f() {}

void (*p)() = f;

int main() {
  return (int) &p;
}
