__thread int a = 0;
typedef unsigned long long size_t;
__attribute__((visibility("hidden"))) extern char __tbss_align[];
size_t foo() { return (size_t)&__tbss_align; }
