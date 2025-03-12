static const char *str1 = "Hello";
static const char *str2 = "World";
extern void bar(const char *);
int a = 10;
int foo() {
  bar(str1);
  bar(str2);
  a = 100;
  return 0;
}
