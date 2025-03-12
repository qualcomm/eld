__thread int tdata1 = 10;
__thread int tdata2 = 10;
__thread int tbss1 = 0;
__thread int tbss2 = 0;

int main() { return tdata1 + tdata2 + tbss2 + tbss2; }
