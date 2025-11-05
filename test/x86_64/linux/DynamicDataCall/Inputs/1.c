long global_var_non_premp = 1;
extern long extern_var;
extern long extern_var2;
void _start() {
  long u = global_var_non_premp + extern_var + extern_var2;
  asm("movq $60, %%rax\n"
      "movq %0, %%rdi\n"
      "syscall\n"
      :
      : "r"(u)
      : "%rax", "%rdi");
}