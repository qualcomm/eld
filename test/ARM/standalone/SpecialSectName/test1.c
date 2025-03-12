static int __attribute__((noinline)) foo(int a) {
	return a*3;
}

__attribute__((section("ENTRY")))
int main (int argc, char **argv) {
	return foo(argc);
}
