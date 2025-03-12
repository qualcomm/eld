int a = 10;
extern int _DYNAMIC;
int foo() { return a + &_DYNAMIC; }
