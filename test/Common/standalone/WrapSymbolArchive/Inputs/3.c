/* add.c: the real add function, placed in an archive.
   The linker must pull this member from the archive when --wrap=add is
   active and __real_add (renamed to 'add') remains undefined. */
int add(int a, int b) { return a + b; }