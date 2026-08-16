__attribute__((section(".randomized_addrs2"), used)) const char sym7[] =
    "second section only";
__attribute__((section(".randomized_addrs2"), used)) const char sym8[] = "hi";

const char *getSym7Ptr() { return sym7; }
const char *getSym8Ptr() { return sym8; }
