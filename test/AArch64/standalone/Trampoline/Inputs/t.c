int __attribute__ ((section (".text.foo"))) foo()
{
  int a = 10;
  return (int) a;
}


int __attribute__ ((section (".text.bar"))) bar()
{
  int a = 100;
  return (int) a;
}
