extern int blah();

int  __attribute__((weak)) QURTK_fatal_hook (void)
{
  return blah();
}

int blah() {
  return 0;
}
