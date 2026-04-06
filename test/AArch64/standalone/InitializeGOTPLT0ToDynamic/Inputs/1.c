int a = 10;
extern int _DYNAMIC;
int foo() { return a + (int)&_DYNAMIC; }
