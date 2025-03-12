extern char *bar;
extern char *bar_alias;
extern char *bar_more_alias;
int main() { return (*bar + *bar_alias + *bar_more_alias) ; }
