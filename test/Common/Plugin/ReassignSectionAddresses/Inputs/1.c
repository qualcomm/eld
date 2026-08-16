__attribute__((section(".randomized_addrs"), used)) const char sym1[] = "hi";
__attribute__((section(".randomized_addrs"), used)) const char sym2[] =
    "a somewhat longer string constant";
__attribute__((section(".randomized_addrs"), used)) const char sym3[] = "x";
__attribute__((section(".randomized_addrs"), used)) const char sym4[] =
    "another string, medium length";
__attribute__((section(".randomized_addrs"), used)) const char sym5[] =
    "yet another distinct string constant, this one considerably longer than "
    "the rest so the section has a real spread of content lengths";
__attribute__((section(".randomized_addrs"), used)) const char sym6[] = "hi";

const char *getSym1Ptr() { return sym1; }
const char *getSym2Ptr() { return sym2; }
const char *getSym3Ptr() { return sym3; }
const char *getSym4Ptr() { return sym4; }
const char *getSym5Ptr() { return sym5; }
const char *getSym6Ptr() { return sym6; }
