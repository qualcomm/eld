#include <unistd.h>

static void emit_char(char c) {
  (void)write(1, &c, 1);
}

__attribute__((destructor)) void fini_default(void) { emit_char('c'); }
