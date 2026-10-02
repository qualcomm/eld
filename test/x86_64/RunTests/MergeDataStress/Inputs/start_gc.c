#include <stdint.h>
extern const void *get_live(void);

static long sys_write(int fd, const void *buf, unsigned long n) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(1), "D"((long)fd), "S"(buf), "d"(n)
                   : "rcx", "r11", "memory");
  return ret;
}

__attribute__((noreturn)) static void sys_exit(int code) {
  __asm__ volatile("syscall"
                   :
                   : "a"(60), "D"((long)code)
                   : "rcx", "r11", "memory");
  __builtin_unreachable();
}

__attribute__((noreturn)) void _start_gc(void) {
  if (!get_live()) {
    static const char msg[] = "RUNTIME_GC_FAIL\n";
    sys_write(1, msg, sizeof(msg) - 1);
    sys_exit(1);
  }
  static const char msg[] = "RUNTIME_GC_OK\n";
  sys_write(1, msg, sizeof(msg) - 1);
  sys_exit(0);
}
