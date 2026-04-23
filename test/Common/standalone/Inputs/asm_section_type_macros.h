/* Token variants for .section/.type directives in preprocessed asm files. */
#if defined(__arm__)
// clang-format off
#define SECTYPE_TOKEN %progbits
#define OBJTYPE_TOKEN %object
// clang-format on
#else
#define SECTYPE_TOKEN @progbits
#define OBJTYPE_TOKEN @object
#endif
