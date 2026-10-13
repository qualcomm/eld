#include <stdio.h>

extern int get_target(void);

int main() {
  printf("target = %d\n", get_target());
  return 0;
}
