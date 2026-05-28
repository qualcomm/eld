#include <unistd.h>

static void emit_char(char c) {
  (void)write(1, &c, 1);
}

int main(void) {
  emit_char('M');
  return 0;
}
