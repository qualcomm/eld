// Test R_X86_64_GOTPCREL relocation
int global_var = 42;

void _start() {
  int result;

  asm volatile("movq global_var@GOTPCREL(%%rip), %%rax\n"
               "movl (%%rax), %%eax\n"
               "movl %%eax, %0\n"
               : "=m"(result)
               :
               : "rax");

  asm volatile("movq $60, %%rax\n"
               "movq %0, %%rdi\n"
               "syscall\n"
               :
               : "r"((long)result)
               : "rax", "rdi");
}
