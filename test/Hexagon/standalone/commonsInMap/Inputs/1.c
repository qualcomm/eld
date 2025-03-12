double common_double;
short common_short;
char common_char;
int common1;
int common2;

int bar(char c, int i) {
  return c + i;
}

int baz() {
  return bar(common_short, common1);
}