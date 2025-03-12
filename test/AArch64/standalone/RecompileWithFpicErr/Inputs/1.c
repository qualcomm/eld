extern __thread int baz;
int foo() { return ++baz;; }
