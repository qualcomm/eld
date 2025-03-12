int z;

void bar() __attribute__((section("mycode_2")));
void bar() {
  z += 1;
}

extern int x,y;
void bar2() {
  z = x+y;
}

