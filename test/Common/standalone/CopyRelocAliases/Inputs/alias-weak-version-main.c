// A non-default weak version must not alias a default strong version in COPY.
extern int old_ref, new_ref;
__asm__(".symver old_ref,mixed@V1");
__asm__(".symver new_ref,mixed@V2");
int main(void) {
  volatile int *a = &old_ref, *b = &new_ref;
  if (a == b)
    return 11;
  if (*a != 0x13579bdf || *b != 0x13579bdf)
    return 12;
  *b = 0x2468ace0;
  if (*a != 0x13579bdf || *b != 0x2468ace0)
    return 13;
  return 0;
}
