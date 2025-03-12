extern int printmystr(char *str);
int callmyprint() {
  return printmystr("foo") + printmystr("bar") + printmystr("baz");
}
