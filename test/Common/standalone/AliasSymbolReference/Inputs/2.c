char *bar = "crap";
extern char *bar_alias __attribute__ ((weak, alias ("bar")));
int foo() { return *bar_alias; }
