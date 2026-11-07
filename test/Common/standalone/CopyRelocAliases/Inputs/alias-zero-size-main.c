#include <stdint.h>
#include <stdio.h>
extern uint32_t payload;
extern void *dso_addr_empty(void);
int main(void) {
  uint32_t *volatile p = &payload;
  void *q = dso_addr_empty();
  printf("payload=%p empty_alias=%p initial=%08x\n", (void *)p, q, *p);
  if ((void *)p != q)
    return 11;
  if (*p != 0x21451234U)
    return 12;
  *p = 0x13579bdfU;
  if (*p != 0x13579bdfU || dso_addr_empty() != (void *)p)
    return 13;
  return 0;
}
