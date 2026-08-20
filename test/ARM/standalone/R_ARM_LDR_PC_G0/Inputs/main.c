#include <stdio.h>

extern int target;
extern int ldr_pc_g0_slot;

int main() {
  printf("target = %d\n", target);
  return target == 42 ? 0 : 1;
}
