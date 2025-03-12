__thread int foo = 10;
__thread int tbss1 = 0;
__thread int tbss2 = 0;
__thread int tbss3 = 0;
__thread int tbss4 = 0;
__thread int tbss5 = 0;
int txt() { return 0; }
__attribute__((aligned(64))) int data = 20;
