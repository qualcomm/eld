int replacement_target = 1;
int *replacements[8192] __attribute__((section(".replacement_data"))) = {
    [0 ... 8191] = &replacement_target};
int main(void) { return 0; }
