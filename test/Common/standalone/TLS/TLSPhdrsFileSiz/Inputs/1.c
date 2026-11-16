__thread int initialized = 1;
__thread int uninitialized;

int main(void) { return initialized + uninitialized; }
