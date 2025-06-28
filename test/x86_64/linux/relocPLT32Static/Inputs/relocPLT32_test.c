int foo() { return 11; }

int val = 2;

void _start() {
  long u = foo() + val;
  asm (
    "movq $60, %%rax\n"
    "movq %0, %%rdi\n"
    "syscall\n"
    :
    : "r" (u)
    : "%rax", "%rdi"
  );
}