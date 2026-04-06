int a = 10;
extern int _DYNAMIC;
int foo() { return a + (int)(long)(&_DYNAMIC); }
