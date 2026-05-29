// Hidden symbol: non-preemptible, direct relocation, no PLT entry.
#define UART0_DR ((volatile unsigned int *)0x10009000)
#define UART0_FR ((volatile unsigned int *)0x10009018)

__attribute__((noinline, visibility("hidden"))) int hidden_callee() {
  return 7;
}

int main(void) {
  if (hidden_callee() != 7) {
    return 1;
  }
  const char *s = "success\n";

  while (*s) {
    while (*UART0_FR & (1 << 5))
      ;

    *UART0_DR = *s++;
  }
  return 0;
}