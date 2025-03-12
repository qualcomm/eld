extern int bar();

int data = 10;
volatile char *ptr = "good";
int foo() { return data + bar() + ptr[1]; }

int main(int argc, char *argv[])
{
	return 0;
}
