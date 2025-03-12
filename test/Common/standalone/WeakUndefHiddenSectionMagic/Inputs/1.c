extern char __start_foo_hidden __attribute__((weak))
__attribute__((visibility("hidden")));

extern char __stop_foo_hidden __attribute__((weak))
__attribute__((visibility("hidden")));

extern char __start_foo_protected __attribute__((weak))
__attribute__((visibility("protected")));

extern char __stop_foo_protected __attribute__((weak))
__attribute__((visibility("protected")));

int main() {
  return (int)&__start_foo_hidden + (int)&__stop_foo_hidden +
         (int)&__start_foo_protected + (int)&__stop_foo_protected;
}
