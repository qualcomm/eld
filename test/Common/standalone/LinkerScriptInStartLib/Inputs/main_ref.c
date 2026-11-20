extern int foo(void);
extern int extra_marker(void);

int main(void) {
  return foo() + extra_marker();
}
