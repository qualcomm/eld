extern int* __start_llvm_profile_data;

int foo() {
  return *__start_llvm_profile_data;
}
