#include <unistd.h>

static void emit_char(char c) {
  (void)write(1, &c, 1);
}

__attribute__((constructor(1000))) void init_low(void) { emit_char('A'); }
