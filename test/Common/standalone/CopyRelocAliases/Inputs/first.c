/* Earlier DSO owns foo_v2 independently of the alias DSO. */
int foo_v2[4] = {99, 98, 97, 96};

int *first_addr_v2(void) { return foo_v2; }
