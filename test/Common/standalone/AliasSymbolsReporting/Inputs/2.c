char *bar = "hello";
extern char *bar_alias __attribute__ ((weak, alias ("bar")));
extern char *bar_more_alias __attribute__ ((weak, alias ("bar_alias")));
int foo() { return *bar; }
