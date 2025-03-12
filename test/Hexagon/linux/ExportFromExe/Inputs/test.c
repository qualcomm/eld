
int variable_in_program = 0;
int common_in_program;

int data_in_program = 1;
extern void func_in_dso(void);

void func_in_program(void)
{
  data_in_program = ++variable_in_program;
}

int main()
{
  func_in_dso();
  return 0;
}
