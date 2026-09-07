int __real_add(int u, int v);

int fn(int u, int v) { return __real_add(u, v); }