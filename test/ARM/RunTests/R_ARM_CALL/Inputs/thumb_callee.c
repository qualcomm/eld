// Thumb target: R_ARM_CALL from ARM caller triggers BL->BLX rewrite.
#define UART0_DR ((volatile unsigned int *)0x10009000)
#define UART0_FR ((volatile unsigned int *)0x10009018)

__attribute__((noinline, target("thumb"))) int thumb_callee() { return 5; }

__attribute__((target("arm"))) int main() {
  if (thumb_callee() != 5) {
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