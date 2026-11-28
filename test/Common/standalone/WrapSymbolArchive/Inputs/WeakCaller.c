extern int add(void) __attribute__((weak));

int main(void) {
  if (add)
    return add();
  return 0;
}
