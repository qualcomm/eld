extern int foo_v1[4];
extern int *first_addr_v2(void);

int main(void) {
  volatile int *first = foo_v1;
  volatile int *second = first_addr_v2();

  return *first == 1 && *second == 99 ? 0 : 1;
}
