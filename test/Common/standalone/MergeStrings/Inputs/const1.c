const char *str1 = "Hello";
const char *str2 = "World";
extern void bar(const char *);
int foo() {
  bar(str1);
  bar(str2);
  return 0;
}
