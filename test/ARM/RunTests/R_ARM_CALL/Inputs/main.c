#define UART0_DR ((volatile unsigned int *)0x10009000)
#define UART0_FR ((volatile unsigned int *)0x10009018)

int main(void) {
  const char *s = "success\n";

  while (*s) {
    while (*UART0_FR & (1 << 5))
      ;
    *UART0_DR = *s++;
  }
  return 0;
}