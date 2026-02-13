__attribute__((section(".text.anchor"), noinline, used)) int anchor_fn(void) {
  return 1;
}

__attribute__((section(".text.beforeA"), noinline, used)) int beforeA_fn(void) {
  return 2;
}

__attribute__((section(".text.beforeB"), noinline, used)) int beforeB_fn(void) {
  return 3;
}

__attribute__((section(".text.afterA"), noinline, used)) int afterA_fn(void) {
  return 4;
}

__attribute__((section(".text.afterB"), noinline, used)) int afterB_fn(void) {
  return 5;
}

__attribute__((section(".text.afterC"), noinline, used)) int afterC_fn(void) {
  return 6;
}

int data_value = 7;

__attribute__((section(".text.anchor"), noinline, used)) int main(void) {
  return anchor_fn() + beforeA_fn() + beforeB_fn() + afterA_fn() + afterB_fn() +
         afterC_fn() + data_value;
}
