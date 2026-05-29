// Caller that uses a preemptible symbol resolved via PLT at runtime.
#include <stdio.h>
extern int shared_callee();

int main() {
  if (shared_callee() != 3) {
    return 1;
  }
  puts("success");
  return 0;
}