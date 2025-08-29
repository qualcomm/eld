int foo() { return 1;}

__attribute__((aligned(8)))
_Thread_local
int u;

__attribute__((aligned(8)))
_Thread_local
int v = 1;

