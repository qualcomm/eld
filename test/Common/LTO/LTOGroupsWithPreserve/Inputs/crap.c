int bar();
int crap()  __attribute__((weak)) {
return bar();
}
