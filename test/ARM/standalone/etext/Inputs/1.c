extern int __etext;
char *a = (char *)&__etext;
int main() __attribute((section(".main.text"))) {
  return 0;
}
