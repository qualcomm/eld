extern void bar();

int main()
{
 bar();
 return 0;
}

__attribute__((visibility ("hidden"))) int foo()
{
  return 1;
}
