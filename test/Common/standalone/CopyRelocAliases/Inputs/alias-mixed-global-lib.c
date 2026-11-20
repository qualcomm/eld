/* Distinct GNU versions share input storage, but must not share COPY storage.
 */
__attribute__((visibility("hidden"))) unsigned int storage = 0x13579bdfU;
extern unsigned int old_impl
    __attribute__((alias("storage"), visibility("default")));
extern unsigned int new_impl
    __attribute__((alias("storage"), visibility("default")));
extern unsigned int other_impl
    __attribute__((alias("storage"), visibility("default")));
__asm__(".symver old_impl,mixed@V1");
__asm__(".symver new_impl,mixed@@V2");
__asm__(".symver other_impl,other@@V2");
