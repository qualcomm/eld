int foo() { return 0; }

static int __attribute__((noinline)) Function1(int a)
{
	return a*3;
}

static int __attribute__((noinline)) Function2(int a)
{
	return a*23;
}


int main (int argc, char **argv)
{
	int a = Function1(argc);
	a += Function2(argc);
	a += Function2(argc+1);
  foo();
	return a;
}
