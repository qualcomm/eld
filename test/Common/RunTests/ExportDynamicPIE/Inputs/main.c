#include <dlfcn.h>
#include <stdio.h>

int exported_var = 42;
int exported_func(void) { return exported_var; }

int main(int argc, char **argv) {
  void *handle = dlopen(argv[1], RTLD_NOW);
  if (!handle) {
    fprintf(stderr, "dlopen error: %s\n", dlerror());
    return 1;
  }

  int (*run_plugin)(void) = dlsym(handle, "run_plugin");
  if (!run_plugin) {
    fprintf(stderr, "dlsym error: %s\n", dlerror());
    return 1;
  }

  int result = run_plugin();
  printf("plugin returned: %d\n", result);
  return 0;
}
