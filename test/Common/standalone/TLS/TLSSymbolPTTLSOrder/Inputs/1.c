__attribute__((section(".tdata.low"))) __thread int low = 1;
__attribute__((section(".tdata.high"), aligned(256))) __thread int high = 2;

int foo(void) { return low; }
