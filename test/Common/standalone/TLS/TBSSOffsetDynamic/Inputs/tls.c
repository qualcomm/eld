__thread int tdataa = 10;
__thread int tdatab = 20;
__thread int tdatac = 30;
__attribute__((aligned(16))) __thread int b = 0;
__attribute__((aligned(16))) __thread int c = 0;
int data = 30;
