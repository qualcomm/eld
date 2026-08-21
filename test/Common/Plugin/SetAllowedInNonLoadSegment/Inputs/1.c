int foo() { return 1; }
int main() { return foo(); }

const int overlay_data __attribute__((section(".overlay"))) = 42;
