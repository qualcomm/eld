int foo(void) __attribute__((ifunc("foo_ifunc")));

static int global = 1;

static int f1(void) { return 0; }

static int f2(void) { return 1; }

void *foo_ifunc(void) { return global == 1 ? f1 : f2; }

int main() { return foo(); }
