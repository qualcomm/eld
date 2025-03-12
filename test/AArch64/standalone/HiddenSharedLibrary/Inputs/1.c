__attribute__ ((visibility ("hidden")))
int fn1() { return 0; }

static int (*a)() = &fn1;
int fn2() { return (*a)();  } 
