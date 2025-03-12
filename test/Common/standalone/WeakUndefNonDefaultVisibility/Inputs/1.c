#ifdef HIDDEN
__attribute__((visibility("hidden"))) __attribute__((weak)) extern int foo();
#else
__attribute__((visibility("protected"))) __attribute__((weak)) extern int foo();
#endif

int main() {
  return foo();
}
