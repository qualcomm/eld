// Distinct symbol versions intentionally share input storage.
__attribute__((visibility("hidden"))) int storage = 0x13579bdf;
extern int old_weak
    __attribute__((weak, alias("storage"), visibility("default")));
extern int new_strong __attribute__((alias("storage"), visibility("default")));
extern int other_alias __attribute__((alias("storage"), visibility("default")));
__asm__(".symver old_weak,mixed@V1");
__asm__(".symver new_strong,mixed@@V2");
__asm__(".symver other_alias,other@@V2");
