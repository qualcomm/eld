__thread int c = 5;
__thread int d = 7;

int bar() { return c + d; }