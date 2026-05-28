#include <unistd.h>

static void emit_char(char c) {
  (void)write(1, &c, 1);
}

__attribute__((destructor(10000))) void fini_high(void) { emit_char('b'); }
