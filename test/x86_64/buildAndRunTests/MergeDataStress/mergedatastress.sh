#REQUIRES: linux, x86
#!/usr/bin/env bash
set -euo pipefail

# Debug notes:
# - %t is the per-test temp directory from lit; inspect it for generated inputs.
# - Rerun verbosely to debug command expansion and output:
#     llvm-lit -a -v <path>/mergedatastress.sh
# - This test validates four scenarios:
#   1) full link merge-on
#   2) full link merge-off
#   3) partial link (-r) must not merge
#   4) gc-sections keeps only live constant
# - Printed output is checked against mergedatastress.golden.
BUILD_DIR=%t
%mkdir -p "$BUILD_DIR"

cat > "$BUILD_DIR/f1.c" <<'SRC'
#include <stdint.h>
__asm__(
  ".section .rodata.cst4,\"aM\",@progbits,4\n"
  ".p2align 2\n"
  ".globl m1\n.type m1,@object\n.size m1,4\n"
  "m1:\n.long 0x11223344\n"
  ".globl d1\n.type d1,@object\n.size d1,4\n"
  "d1:\n.long 0x55667788\n"
  ".section .rodata.cst8,\"aM\",@progbits,8\n"
  ".p2align 3\n"
  ".globl e1\n.type e1,@object\n.size e1,8\n"
  "e1:\n.quad 0x0102030405060708\n"
  ".section .rodata,\"a\",@progbits\n"
  ".p2align 2\n"
  ".globl n1\n.type n1,@object\n.size n1,4\n"
  "n1:\n.long 0x11223344\n");
SRC

cat > "$BUILD_DIR/f2.c" <<'SRC'
#include <stdint.h>
__asm__(
  ".section .rodata.cst4,\"aM\",@progbits,4\n"
  ".p2align 4\n"
  ".globl m2\n.type m2,@object\n.size m2,4\n"
  "m2:\n.long 0x11223344\n"
  ".globl d2\n.type d2,@object\n.size d2,4\n"
  "d2:\n.long 0x99aabbcc\n"
  ".section .rodata.cst8,\"aM\",@progbits,8\n"
  ".p2align 3\n"
  ".globl e2\n.type e2,@object\n.size e2,8\n"
  "e2:\n.quad 0x0102030405060708\n"
  ".section .rodata,\"a\",@progbits\n"
  ".p2align 2\n"
  ".globl n2\n.type n2,@object\n.size n2,4\n"
  "n2:\n.long 0x11223344\n");
SRC

cat > "$BUILD_DIR/f3.c" <<'SRC'
#include <stdint.h>
__asm__(
  ".section .rodata.cst4,\"aM\",@progbits,4\n"
  ".p2align 2\n"
  ".globl m3\n.type m3,@object\n.size m3,4\n"
  "m3:\n.long 0x11223344\n"
  ".globl x3\n.type x3,@object\n.size x3,4\n"
  "x3:\n.long 0xdeadbeef\n");
SRC

cat > "$BUILD_DIR/runtime_common.c" <<'SRC'
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
  __asm__ volatile("syscall" : "=a"(ret) : "a"(1), "D"((long)fd), "S"(buf), "d"(n) : "rcx", "r11", "memory");
  return ret;
}

__attribute__((noreturn)) static void sys_exit(int code) {
  __asm__ volatile("syscall" : : "a"(60), "D"((long)code) : "rcx", "r11", "memory");
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
SRC

cat > "$BUILD_DIR/gc_live.c" <<'SRC'
#include <stdint.h>
__asm__(
  ".section .rodata.cst4.live,\"aM\",@progbits,4\n"
  ".p2align 2\n"
  ".globl c_gc_live\n.type c_gc_live,@object\n.size c_gc_live,4\n"
  "c_gc_live:\n.long 0xabcdef01\n");
extern const uint32_t c_gc_live;
__attribute__((section(".text.get_live"))) const void *get_live(void) { return &c_gc_live; }
SRC

cat > "$BUILD_DIR/gc_dead.c" <<'SRC'
#include <stdint.h>
__asm__(
  ".section .rodata.cst4.dead,\"aM\",@progbits,4\n"
  ".p2align 2\n"
  ".globl c_gc_dead\n.type c_gc_dead,@object\n.size c_gc_dead,4\n"
  "c_gc_dead:\n.long 0x13572468\n");
extern const uint32_t c_gc_dead;
__attribute__((section(".text.get_dead"))) const void *get_dead(void) { return &c_gc_dead; }
SRC

cat > "$BUILD_DIR/start_gc.c" <<'SRC'
#include <stdint.h>
extern const void *get_live(void);

static long sys_write(int fd, const void *buf, unsigned long n) {
  long ret;
  __asm__ volatile("syscall" : "=a"(ret) : "a"(1), "D"((long)fd), "S"(buf), "d"(n) : "rcx", "r11", "memory");
  return ret;
}

__attribute__((noreturn)) static void sys_exit(int code) {
  __asm__ volatile("syscall" : : "a"(60), "D"((long)code) : "rcx", "r11", "memory");
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
SRC

%clang %clangopts -ffreestanding -fno-pic -c "$BUILD_DIR/f1.c" -o "$BUILD_DIR/f1.o"
%clang %clangopts -ffreestanding -fno-pic -c "$BUILD_DIR/f2.c" -o "$BUILD_DIR/f2.o"
%clang %clangopts -ffreestanding -fno-pic -c "$BUILD_DIR/f3.c" -o "$BUILD_DIR/f3.o"
%clang %clangopts -ffreestanding -fno-pic -c "$BUILD_DIR/runtime_common.c" -o "$BUILD_DIR/runtime_common.o"
%clang %clangopts -ffreestanding -fno-pic -ffunction-sections -fdata-sections -c "$BUILD_DIR/gc_live.c" -o "$BUILD_DIR/gc_live.o"
%clang %clangopts -ffreestanding -fno-pic -ffunction-sections -fdata-sections -c "$BUILD_DIR/gc_dead.c" -o "$BUILD_DIR/gc_dead.o"
%clang %clangopts -ffreestanding -fno-pic -ffunction-sections -fdata-sections -c "$BUILD_DIR/start_gc.c" -o "$BUILD_DIR/start_gc.o"

# Full link with default merge behavior should pass runtime checks.
%eld --entry=_start_merge_on "$BUILD_DIR/runtime_common.o" "$BUILD_DIR/f1.o" "$BUILD_DIR/f2.o" "$BUILD_DIR/f3.o" -o "$BUILD_DIR/full-merge-on.elf"
merge_on_out="$BUILD_DIR/merge_on.out"
%run "$BUILD_DIR/full-merge-on.elf" | tee "$merge_on_out"

# Full link with --no-merge-constants should also pass (with opposite expectations).
%eld --no-merge-constants --entry=_start_merge_off "$BUILD_DIR/runtime_common.o" "$BUILD_DIR/f1.o" "$BUILD_DIR/f2.o" "$BUILD_DIR/f3.o" -o "$BUILD_DIR/full-merge-off.elf"
merge_off_out="$BUILD_DIR/merge_off.out"
%run "$BUILD_DIR/full-merge-off.elf" | tee "$merge_off_out"

# Partial link: identical constants should remain unmerged at -r stage.
%eld -r "$BUILD_DIR/f1.o" "$BUILD_DIR/f2.o" "$BUILD_DIR/f3.o" -o "$BUILD_DIR/partial-r.o"

m1=$(%readelf -s -W "$BUILD_DIR/partial-r.o" | awk '$NF=="m1" {v=$2} END {print v}')
m2=$(%readelf -s -W "$BUILD_DIR/partial-r.o" | awk '$NF=="m2" {v=$2} END {print v}')
if [[ -n "$m1" && "$m1" == "$m2" ]]; then
  echo "FAIL: partial link unexpectedly merged constants" >&2
  exit 1
fi

# GC case: live symbol must exist; dead symbol must be removed.
%eld --gc-sections --entry=_start_gc "$BUILD_DIR/start_gc.o" "$BUILD_DIR/gc_live.o" "$BUILD_DIR/gc_dead.o" -o "$BUILD_DIR/gc.elf"
gc_out="$BUILD_DIR/gc.out"
%run "$BUILD_DIR/gc.elf" | tee "$gc_out"

%readelf -s -W "$BUILD_DIR/gc.elf" | %grep ' c_gc_live$' >/dev/null
%readelf -s -W "$BUILD_DIR/gc.elf" | %grep ' c_gc_dead$' && exit 1 || true

{
  cat "$merge_on_out"
  cat "$merge_off_out"
  cat "$gc_out"
  echo "PASS: constant-data merge stress test completed"
} | tee "$BUILD_DIR/mergedatastress.out"
%filecheck %p/mergedatastress.golden < "$BUILD_DIR/mergedatastress.out"
#CHECK: PASS: constant-data merge stress test completed
