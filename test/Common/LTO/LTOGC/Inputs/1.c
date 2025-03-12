int foo_sym() { return 1; }

int bar_sym() { return 3; }

int baz_sym() { return bar_sym() + 5; }

int foobar_sym() { return foo_sym() + 7; }

int asdf_sym() { return 9; }