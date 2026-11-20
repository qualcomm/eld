#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;
extern char **pic_env(void);
extern char ***pic_addr(void);

__attribute__((noinline)) char ***nonpic_addr(void) { return &environ; }

static int has_marker(char **entries) {
  if (!entries)
    return 0;
  for (int i = 0; i < 4096 && entries[i]; ++i)
    if (!strcmp(entries[i], "ELD2145_S11_MARKER=present"))
      return 1;
  return 0;
}

int main(void) {
  char ***weak_sym = dlsym(RTLD_DEFAULT, "environ");
  char ***strong_sym = dlsym(RTLD_DEFAULT, "__environ");
  if (!weak_sym || !strong_sym || weak_sym != strong_sym ||
      weak_sym != pic_addr() || weak_sym != nonpic_addr())
    return 11;
  if (setenv("ELD2145_S11_MARKER", "present", 1) != 0)
    return 12;
  if (!has_marker(environ) || !has_marker(pic_env()) ||
      !has_marker(*strong_sym))
    return 13;
  puts("PASSED");
  return 0;
}
