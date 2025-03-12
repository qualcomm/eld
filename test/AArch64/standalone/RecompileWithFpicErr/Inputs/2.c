__thread int baz;
extern int foo();
int bazVal() { return baz+foo(); }

