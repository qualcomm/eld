static int bar;
int *foo = &bar;
int baz() { return (*foo); }
