// Caller that invokes a distant function
// This requires a trampoline to be created by the linker

#define UART0_DR ((volatile unsigned int *)0x10009000)
#define UART0_FR ((volatile unsigned int *)0x10009018)

extern int distant_callee(void);

int main(void) {
  int result = distant_callee();

  // Print success if we got here (trampoline worked)
  const char *s = "success\n";

  while (*s) {
    while (*UART0_FR & (1 << 5))
      ;

    *UART0_DR = *s++;
  }
  return result;
}