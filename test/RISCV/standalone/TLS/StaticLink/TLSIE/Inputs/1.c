__thread int bar = 10;
__thread int baz = 20;
__thread int far = 30;
__thread int car = 30;
int foo() { return bar + baz + far + car; }
