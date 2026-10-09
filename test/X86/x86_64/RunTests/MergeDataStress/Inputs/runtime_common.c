#include <stdint.h>
extern const uint32_t m1, m2, m3, d1, d2, n1, n2;
extern const uint64_t e1, e2;

const void *get_m1(void) { return &m1; }
const void *get_m2(void) { return &m2; }
const void *get_m3(void) { return &m3; }
const void *get_d1(void) { return &d1; }
const void *get_d2(void) { return &d2; }
const void *get_n1(void) { return &n1; }
const void *get_n2(void) { return &n2; }
const void *get_e1(void) { return &e1; }
const void *get_e2(void) { return &e2; }

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

__attribute__((noreturn)) void _start_merge_on(void) {
  if (get_m1() != get_m2() || get_m2() != get_m3() || get_e1() != get_e2() ||
      get_d1() == get_d2() || get_n1() == get_n2() || get_n1() == get_m1()) {
    static const char msg[] = "RUNTIME_MERGE_ON_FAIL\n";
    sys_write(1, msg, sizeof(msg) - 1);
    sys_exit(1);
  }
  static const char msg[] = "RUNTIME_MERGE_ON_OK\n";
  sys_write(1, msg, sizeof(msg) - 1);
  sys_exit(0);
}

__attribute__((noreturn)) void _start_merge_off(void) {
  if (get_m1() == get_m2() || get_m2() == get_m3() || get_e1() == get_e2() ||
      get_d1() == get_d2() || get_n1() == get_n2()) {
    static const char msg[] = "RUNTIME_MERGE_OFF_FAIL\n";
    sys_write(1, msg, sizeof(msg) - 1);
    sys_exit(1);
  }
  static const char msg[] = "RUNTIME_MERGE_OFF_OK\n";
  sys_write(1, msg, sizeof(msg) - 1);
  sys_exit(0);
}
