// Weak undefined symbol: linker rewrites BL to NOP.
// The call must be silently skipped and execution must continue normally.
#define UART0_DR ((volatile unsigned int *)0x10009000)
#define UART0_FR ((volatile unsigned int *)0x10009018)

__attribute__((weak)) void weak_undef_func();

int main(void) {
  weak_undef_func(); // must not crash - linker converts BL to NOP
  const char *s = "success\n";

  while (*s) {
    while (*UART0_FR & (1 << 5))
      ;

    *UART0_DR = *s++;
  }
  return 0;
}