int foo_data[4] = {1, 2, 3, 4};
int dead_data[4] = {5, 6, 7, 8};
int *reloc_data[2] = {&foo_data[0], &foo_data[2]};

int main() { return *reloc_data[0]; }
