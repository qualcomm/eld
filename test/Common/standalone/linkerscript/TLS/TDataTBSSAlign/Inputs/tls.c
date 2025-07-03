__thread int a = 1;
__attribute__((aligned(256))) __thread long long b;

int main(int argc, char *argv[]) { return 0; }
