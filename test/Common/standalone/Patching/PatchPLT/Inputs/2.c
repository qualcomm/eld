void __attribute__((patchable)) f();

void f() {}

int main() {
  return (int) f;
}
