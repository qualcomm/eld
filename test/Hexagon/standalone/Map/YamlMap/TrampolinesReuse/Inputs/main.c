

__attribute__((section(".tcm_static")))
int __attribute__((noinline)) Function1(int a)
{
	return a*3;
}

__attribute__((section(".tcm_static")))
int __attribute__((noinline)) Function2(int a)
{
	return a*23;
}


int main (int argc, char **argv)
{
	int a = Function1(argc);
	a += Function2(argc);
	a += Function2(argc+1);
	return a;
}
