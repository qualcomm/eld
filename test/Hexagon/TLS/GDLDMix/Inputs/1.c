extern int bar();
extern __thread int src1;

int ptr;

int foo() {
  ptr = src1 + bar();
  return ptr;
}
