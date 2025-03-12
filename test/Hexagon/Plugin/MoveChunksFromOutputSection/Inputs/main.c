#include <stdio.h>

__attribute__((section(".text.hotf1"))) int hotf1() { return 0; }
__attribute__((section(".text.hotf2"))) int hotf2() { return 0; }
__attribute__((section(".text.hotf3"))) int hotf3() { return 0; }
__attribute__((section(".text.hotf4"))) int hotf4() { return 0; }
__attribute__((section(".text.hotf5"))) int hotf5() { return 0; }
__attribute__((section(".text.hotf6"))) int hotf6() { return 0; }

__attribute__((section(".text.coldf1"))) int coldf1() { return 0; }
__attribute__((section(".text.coldf2"))) int coldf2() { return 0; }
__attribute__((section(".text.coldf3"))) int coldf3() { return 0; }
__attribute__((section(".text.coldf4"))) int coldf4() { return 0; }
__attribute__((section(".text.coldf5"))) int coldf5() { return 0; }
__attribute__((section(".text.coldf6"))) int coldf6() { return 0; }

__attribute__((aligned(512))) int unlikelyf1() { return 0; }
__attribute__((aligned(512))) int unlikelyf2() { return 0; }
__attribute__((aligned(512))) int unlikelyf3() { return 0; }
__attribute__((aligned(512))) int unlikelyf4() { return 0; }
__attribute__((aligned(512))) int unlikelyf5() { return 0; }
__attribute__((aligned(512))) int unlikelyf6() { return 0; }

__attribute__((section(".text.myhotf1"))) int myhotf1() { return 0; }
__attribute__((section(".text.myhotf2"))) int myhotf2() { return 0; }
__attribute__((section(".text.myhotf3"))) int myhotf3() { return 0; }
__attribute__((section(".text.myhotf4"))) int myhotf4() { return 0; }
__attribute__((section(".text.myhotf5"))) int myhotf5() { return 0; }
__attribute__((section(".text.myhotf6"))) int myhotf6() { return 0; }

extern int printmystr(char *str);

int main() {
  int hot = hotf1() + hotf2() + hotf3() + hotf4() + hotf5() + hotf6();
  int myhot = myhotf1() + myhotf2() + myhotf3() + myhotf4() + myhotf5() + myhotf6();
  int cold = coldf1() + coldf2() + coldf3() + coldf4() + coldf5() + coldf6();
  int unlikely = unlikelyf1() + unlikelyf2() + unlikelyf3() + unlikelyf4() +
                 unlikelyf5() + unlikelyf6() + myhot;
  int str = printmystr("foo") + printmystr("bar") + printmystr("baz");
  return 0;
}
