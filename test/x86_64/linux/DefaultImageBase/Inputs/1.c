void _start() {
  long u = 7;
  asm (
    "movq $60, %%rax\n"
    "movq %0, %%rdi\n"
    "syscall\n"
    :
    : "r" (u)
    : "%rax", "%rdi"
  );
}
