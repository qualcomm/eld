extern int baz_impl();

int bar_calls_baz() { return baz_impl() + 2; }
