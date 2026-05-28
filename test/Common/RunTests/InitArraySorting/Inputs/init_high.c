#include <unistd.h>

static void emit_char(char c) {
  (void)write(1, &c, 1);
}

__attribute__((constructor(10000))) void init_high(void) { emit_char('B'); }
