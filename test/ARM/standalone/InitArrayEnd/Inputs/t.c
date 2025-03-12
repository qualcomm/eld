int foo;
extern int __init_array_start;
extern int __init_array_end;
__attribute__((constructor)) static void _bar() { foo = 3; }

int main()
{
  return (int) __init_array_end - __init_array_start;
}