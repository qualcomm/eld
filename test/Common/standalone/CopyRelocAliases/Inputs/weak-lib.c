__attribute__((visibility("hidden"))) int storage[4] = {1, 2, 3, 4};

extern int strong_alias[4]
    __attribute__((alias("storage"), visibility("default")));

extern int weak_alias[4]
    __attribute__((weak, alias("storage"), visibility("default")));
