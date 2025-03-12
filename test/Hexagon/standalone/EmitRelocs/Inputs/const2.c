static const char *str1 = "World";
static const char *str2 = "Hello";
extern void bar(const char *);
int baz() {
  bar(str1);
  bar(str2);
  return 0;
}
