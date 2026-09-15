#include <stdio.h>

__thread int tval = 42;

int get_tval(void) { return tval; }
