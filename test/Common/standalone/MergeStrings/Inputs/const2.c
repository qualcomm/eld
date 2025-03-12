const char *str3 = "World";
const char *str4 = "Hello";
extern void bar(const char *);
int baz() {
  bar(str3);
  bar(str4);
  return 0;
}

void bar(const char *a) { return;}
