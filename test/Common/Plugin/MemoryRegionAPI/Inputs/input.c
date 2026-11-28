int initialized = 7;
int uninitialized;
int explicit_data __attribute__((section(".explicit"))) = 8;
int alias_data __attribute__((section(".alias"))) = 9;
int auto_data __attribute__((section(".auto"))) = 10;
void _start(void) {}
