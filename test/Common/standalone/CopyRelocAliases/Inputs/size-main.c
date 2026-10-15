extern unsigned char small_alias;

/* Only the smaller alias is referenced by the executable. */
void *get_small_address(void) { return &small_alias; }
