#include <stdio.h>

int variant(int x) { return x + 100; }
int entry(int);

int main(void)
{
    int r = entry(1);
    printf("entry(1)=%d -> %s variant\n", r, r == 202 ? "program" : "library");
    return r == 202 ? 0 : 1;
}
