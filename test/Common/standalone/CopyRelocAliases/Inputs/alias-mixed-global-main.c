extern unsigned int old_ref, new_ref, other;
__asm__(".symver old_ref,mixed@V1");
__asm__(".symver new_ref,mixed@V2");

int main(void) {
  unsigned int *volatile old_addr = &old_ref;
  unsigned int *volatile new_addr = &new_ref;
  unsigned int *volatile other_addr = &other;
  if (old_addr == new_addr || new_addr != other_addr)
    return 11;
  if (*old_addr != 0x13579bdfU || *new_addr != 0x13579bdfU ||
      *other_addr != 0x13579bdfU)
    return 12;
  *new_addr = 0x2468ace0U;
  if (*old_addr != 0x13579bdfU || *other_addr != 0x2468ace0U)
    return 13;
  return 0;
}
