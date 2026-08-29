/* wrap.c: the __wrap_add function that calls __real_add.
   When --wrap=add is used, IRBuilder renames the undefined reference to
   __real_add into 'add'.  This is the reference that must trigger pulling
   add.o out of the archive. */
extern int __real_add(int a, int b);
int __wrap_add(int a, int b) {
  return __real_add(a, b) * 2;
}
