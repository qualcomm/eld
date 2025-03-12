#include <stdio.h>

const char *HelloWorld() {
  return "Hello World!";
}

const char *HelloQualcomm() {
  return "Hello Qualcomm!";
}

int main() {
  printf("%s\n", HelloWorld());
}