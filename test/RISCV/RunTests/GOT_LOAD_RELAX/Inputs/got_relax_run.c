#include <stdio.h>

int relative_var = 12345;

extern int sym_cli;
extern int sym_addi;

__attribute__((noinline)) static int *get_relative(void) {
  return &relative_var;
}
__attribute__((noinline)) static void *get_cli(void) { return &sym_cli; }
__attribute__((noinline)) static void *get_addi(void) { return &sym_addi; }

int main() {
  printf("relative: %d\n", *get_relative());
  printf("cli: %p\n", get_cli());
  printf("addi: %p\n", get_addi());
  return 0;
}
