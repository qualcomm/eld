extern __thread int src1;
extern __thread int src2;
__thread int dst;

int *ptr;

int foo1 () {
  ptr = &dst;
  *ptr = src1 + src2;
  return *ptr;
}
