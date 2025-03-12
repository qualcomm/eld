extern int foo() __attribute__ ((visibility ("hidden")));

int bar() {
 return foo - bar;
}
