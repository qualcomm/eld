static int LocalVal = 43;
static int *LocalPtr = &LocalVal;
int GlobalVal = 42;
int *GlobalPtr = &GlobalVal;

int main(void) { return *LocalPtr - *GlobalPtr - 1; }
