
#include <stdio.h>

int tentative;
char small='a';

int main() __attribute__((section(".text.main")));
int main()
{
  printf("Hello World\n");
  return 0;
}
