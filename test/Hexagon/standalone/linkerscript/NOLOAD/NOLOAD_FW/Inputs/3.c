int foo() {
  return 0;
}
__attribute__((aligned(16384))) int bss1[100] = { 0 };
__attribute__((aligned(16384))) int bss2[100] = { 0 };
__attribute__((aligned(16384))) int bss3[100] = { 0 };
__attribute__((aligned(16384))) int abss1[100] = { 0 };
__attribute__((aligned(16384))) int abss2[100] = { 0 };
__attribute__((aligned(16384))) int abss3[100] = { 0 };

int bar() {
  return 0;
}
