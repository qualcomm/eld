int u = 11;
static int *v = &u;  // Should emit R_X86_64_64
static int **p = &v; // Should emit R_X86_64_RELATIVE

int foo() {
  u = 13;
  return u + *v + **p; // Returns 39 (13 + 13 + 13)
}