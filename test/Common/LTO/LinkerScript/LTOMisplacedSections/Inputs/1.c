__attribute__((section(".rodata.island"))) volatile static const int foo[100] = {0};
__attribute__((section(".rodata.bar"))) volatile static const int bar[100] = {0};
int main() { return foo[0] + bar[0]; }
