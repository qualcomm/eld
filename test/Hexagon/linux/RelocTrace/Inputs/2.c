extern  int main_1();

int main_2() __attribute__((section("main2")));
int a2 = 2;
int main_2() { return main_1(); }
