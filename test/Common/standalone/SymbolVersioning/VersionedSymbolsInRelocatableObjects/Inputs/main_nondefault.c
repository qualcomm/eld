__asm__(".symver bar_v1, bar@V1");
extern int bar_v1(void);
int main(void) { return bar_v1(); }
