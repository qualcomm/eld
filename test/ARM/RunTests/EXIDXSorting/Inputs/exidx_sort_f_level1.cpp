// exidx_sort_f_level1.cpp — separate translation unit for per-section EXIDX
// test. Compiled with -ffunction-sections so each function gets its own
// .text.funcname / .ARM.exidx.text.funcname pair.
int level2();
__attribute__((noinline)) int level1() { return level2(); }
