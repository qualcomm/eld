#if defined(__arm__)
#define SECTYPE "%progbits"
#define OBJTYPE "%object"
#else
#define SECTYPE "@progbits"
#define OBJTYPE "@object"
#endif
