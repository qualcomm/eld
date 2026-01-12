int foo() { return 1;}

int empty_var1;
int empty_var2;
__thread int a;
__thread int b;
__thread int c;
__thread int d;
__thread int e;
__attribute__((section(".sdata.v"))) int v = 11;
__attribute__((section(".sdata.w"))) int w = 13;
