#include <unistd.h>

static void emit_char(char c) {
  (void)write(1, &c, 1);
}

__attribute__((destructor(1000))) void fini_low(void) {
  emit_char('a');
  emit_char('\n');
}
