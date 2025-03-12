char a[0x100] __attribute__((section(".bss.A")));
char b[0x200] __attribute__((section(".bss.B")));
char c[0x300] __attribute__((section("data1")));
char d[0x400] __attribute__((section("data2")));
int main() {}
