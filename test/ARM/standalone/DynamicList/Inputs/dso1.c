static int bar() { return 0; }
static int car() { return 0; }
int far() { return 0; }
int foo() { return bar(); }
int mar() { return 0; }
int (*baz)() = bar;
int (*boo)() = car;
int (*moo)() = far;
int (*zoo)() = mar;

extern int func_exe();
int func_dso() {
return far() + func_exe();
}
