extern void bar();

int main()
{
 bar();
 return 0;
}

__attribute__((weak))  int foo()
{
  return 1;
}
