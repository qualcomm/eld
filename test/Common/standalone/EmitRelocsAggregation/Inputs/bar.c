int z;

void bar(void) __attribute__((section("mycode_2")));
void bar(void) { z += 1; }

extern int x;
extern int y;
void bar2(void) { z = x + y; }
