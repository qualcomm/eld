volatile static int bar1 = 0;
volatile static int bar2 = 1;
volatile static int bar3 = 2;
volatile static int bar4 = 3;
int common1;
int common2;
int data = 10;

int foo() { return data+bar1+bar2+bar3+bar4+common1+common2; }
