__attribute__((noinline)) static void bar(volatile int*a){*a=1;}
__attribute__((target("thumb-mode"))) void foo(volatile int *a){bar(a);}
__attribute__((target("thumb-mode"))) void baz(volatile int *a){bar(a);}
