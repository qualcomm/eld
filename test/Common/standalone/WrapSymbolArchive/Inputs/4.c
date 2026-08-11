/* wrap.c: the __wrap_add function that never calls __real_add.
   when __wrap_add is defined but never calls __real_add,
   the archive member is not required and linking succeeds cleanly.. */
int __wrap_add(int a, int b) { return (a + b) * 2; }