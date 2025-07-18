__attribute__((ifunc("resolve_bar")))
void bar(void);
void my_bar(void) { }
void (*resolve_bar(void)) (void) { return my_bar; }
