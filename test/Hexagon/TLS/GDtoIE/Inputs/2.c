__thread int src2 = 10;
__thread int src1 = 10;
static __thread int staticdata = 10;

int bar() {
  return src2 + staticdata;
}
