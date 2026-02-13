__attribute__((section(".text.anchor"), noinline, used)) int anchor_fn(void) {
  return 1;
}

__attribute__((section(".text.before"), noinline, used)) int before_fn(void) {
  return 2;
}

__attribute__((section(".text.after1"), noinline, used)) int after1_fn(void) {
  return 3;
}

__attribute__((section(".text.after2"), noinline, used)) int after2_fn(void) {
  return 4;
}

int data_value = 5;

__attribute__((section(".text.anchor"), noinline, used)) int main(void) {
  return anchor_fn() + before_fn() + after1_fn() + after2_fn() + data_value;
}
