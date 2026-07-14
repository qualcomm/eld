#include <stdio.h>

__thread int a = 52;
__thread int b = 41;

__attribute__((noinline)) static int get_a(void) { return a; }
__attribute__((noinline)) static int get_b(void) { return b; }
__attribute__((noinline)) static int set_b(int n) { return b = n; }
__attribute__((noinline)) static int get_a_plus_b(void) { return a + b; }

int main() {
  printf("a: %d\n", get_a());
  printf("b: %d\n", get_b());
  printf("a + b: %d\n", get_a_plus_b());
  set_b(get_a_plus_b());
  printf("b2: %d\n", get_b());
  return 0;
}