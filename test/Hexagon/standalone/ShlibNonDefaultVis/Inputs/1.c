/* Resolve a normal symbol foo with a protected symbol, by creating a PLT */
extern int foo();
int callfoo() { return foo(); }
