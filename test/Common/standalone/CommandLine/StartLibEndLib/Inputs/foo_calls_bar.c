extern int bar_calls_baz();

int foo_calls_bar() { return bar_calls_baz() + 1; }
