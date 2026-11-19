__thread int b;
int data = 1;

int get(void) { return b; }

int main(void) { return get(); }
