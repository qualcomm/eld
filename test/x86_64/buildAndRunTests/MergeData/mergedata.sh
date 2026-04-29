#REQUIRES: linux, x86
#!/usr/bin/env bash
set -euo pipefail

# Debug notes:
# - lit expands %t to a per-test temp directory; all generated files live there.
# - If this test fails, rerun with:
#     llvm-lit -a -v <path>/mergedata.sh
#   to see the expanded commands and tool output.
# - The runtime assertion is simple: merge_test.elf must exit 0.
# - Printed output is checked against mergedata.golden.
BUILD_DIR=%t
%mkdir -p "$BUILD_DIR"

cat > "$BUILD_DIR/a.s" <<'AS'
  .section .rodata.cst4,"aM",@progbits,4
  .globl cst_a
cst_a:
  .long 0x4048f5c3

  .text
  .globl get_a
get_a:
  leaq cst_a(%rip), %rax
  ret
AS

cat > "$BUILD_DIR/b.s" <<'AS'
  .section .rodata.cst4,"aM",@progbits,4
  .globl cst_b
cst_b:
  .long 0x4048f5c3

  .text
  .globl get_b
get_b:
  leaq cst_b(%rip), %rax
  ret
AS

cat > "$BUILD_DIR/start.s" <<'AS'
  .text
  .globl _start
_start:
  call get_a
  movq %rax, %rbx
  call get_b
  cmpq %rax, %rbx
  jne .Lfail

  movl $0, %edi
  jmp .Lexit

.Lfail:
  movl $1, %edi

.Lexit:
  movl $60, %eax
  syscall
AS

%clang %clangopts -c "$BUILD_DIR/a.s" -o "$BUILD_DIR/a.o"
%clang %clangopts -c "$BUILD_DIR/b.s" -o "$BUILD_DIR/b.o"
%clang %clangopts -c "$BUILD_DIR/start.s" -o "$BUILD_DIR/start.o"

# Link with ld.eld and then execute. Exit code 0 means constants merged.
%eld --entry=_start "$BUILD_DIR/start.o" "$BUILD_DIR/a.o" "$BUILD_DIR/b.o" -o "$BUILD_DIR/merge_test.elf"

# Optional breadcrumb: keep this grep output in logs to inspect rodata layout.
%readelf -S -W "$BUILD_DIR/merge_test.elf" | %grep -E '\.rodata|Name|Flg' || true
{
  %run "$BUILD_DIR/merge_test.elf"
  echo "PASS: constants merged"
} | tee "$BUILD_DIR/mergedata.out"
%filecheck %p/mergedata.golden < "$BUILD_DIR/mergedata.out"
#CHECK: PASS: constants merged
