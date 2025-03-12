extern int variable_in_program;
extern int common_in_program;
extern int data_in_program;

extern void func_in_program(void);

void func_in_dso(void)
{
  variable_in_program++;
  common_in_program = variable_in_program + data_in_program;
  func_in_program();
}
