extern int foo_v1;
extern int foo_v2;

int main(void) {
  int *volatile a = &foo_v1;
  int *volatile b = &foo_v2;

  return a == b || *a != 99 || *b != 42;
}
