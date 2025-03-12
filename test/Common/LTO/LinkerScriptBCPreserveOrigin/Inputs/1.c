int zz __attribute__((section(".rodata.lsy.z"))) = 99;
int a[1000];
int baz () __attribute__((section(".text.lsy.baz")));
extern int foo ();
int baz () {
a[0] = foo(101);
  return a[0] + zz;
}
