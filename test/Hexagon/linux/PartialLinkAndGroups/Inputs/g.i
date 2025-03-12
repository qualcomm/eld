# 1 "g.c"
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 291 "<built-in>" 3
# 1 "<command line>" 1
# 1 "<built-in>" 2
# 1 "g.c" 2
extern int blah();

int __attribute__((weak)) QURTK_fatal_hook (void)
{
  return blah();
}

int blah() {
  return 0;
}
