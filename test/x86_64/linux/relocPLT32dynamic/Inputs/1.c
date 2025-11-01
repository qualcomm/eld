int foo();
int foo2();
void _start() {
  long u = foo() + foo2();
  asm("movq $60, %%rax\n"
      "movq %0, %%rdi\n"
      "syscall\n"
      :
      : "r"(u)
      : "%rax", "%rdi");
}