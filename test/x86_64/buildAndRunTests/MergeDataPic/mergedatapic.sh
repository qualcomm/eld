#REQUIRES: linux, x86
#!/usr/bin/env bash
set -euo pipefail

# Debug notes:
# - Artifacts are emitted under %t (expanded by lit to a temp directory).
# - Rerun with verbosity when debugging:
#     llvm-lit -a -v <path>/mergedatapic.sh
# - Expectations:
#     merge_on.elf  -> exit 0
#     merge_off.elf -> non-zero (checked via %not)
# - Printed output is checked against mergedatapic.golden.
BUILD_DIR=%t
%mkdir -p "$BUILD_DIR"

cat > "$BUILD_DIR/a_const.s" <<'SRC'
  .section .rodata.cst4,"aM",@progbits,4
  .globl c_a
  .type c_a, @object
  .size c_a, 4
c_a:
  .long 0x4048f5c3
SRC

cat > "$BUILD_DIR/b_const.s" <<'SRC'
  .section .rodata.cst4,"aM",@progbits,4
  .globl c_b
  .type c_b, @object
  .size c_b, 4
c_b:
  .long 0x4048f5c3
SRC

cat > "$BUILD_DIR/a_get.c" <<'SRC'
extern const unsigned c_a;
const unsigned *get_a(void) { return &c_a; }
SRC

cat > "$BUILD_DIR/b_get.c" <<'SRC'
extern const unsigned c_b;
const unsigned *get_b(void) { return &c_b; }
SRC

cat > "$BUILD_DIR/start.s" <<'SRC'
  .text
  .globl _start
_start:
  call get_a
  movq %rax, %rbx
  call get_b
  cmpq %rax, %rbx
  jne .Lnot_merged

  movl $0, %edi
  jmp .Lexit

.Lnot_merged:
  movl $1, %edi

.Lexit:
  movl $60, %eax
  syscall
SRC

%clang %clangopts -c "$BUILD_DIR/a_const.s" -o "$BUILD_DIR/a_const.o"
%clang %clangopts -c "$BUILD_DIR/b_const.s" -o "$BUILD_DIR/b_const.o"
%clang %clangopts -fPIC -ffreestanding -c "$BUILD_DIR/a_get.c" -o "$BUILD_DIR/a_get.o"
%clang %clangopts -fPIC -ffreestanding -c "$BUILD_DIR/b_get.c" -o "$BUILD_DIR/b_get.o"
%clang %clangopts -c "$BUILD_DIR/start.s" -o "$BUILD_DIR/start.o"

# Case 1: default merge behavior should merge equal constants.
%eld --entry=_start "$BUILD_DIR/start.o" "$BUILD_DIR/a_const.o" "$BUILD_DIR/b_const.o" "$BUILD_DIR/a_get.o" "$BUILD_DIR/b_get.o" -o "$BUILD_DIR/merge_on.elf"
%run "$BUILD_DIR/merge_on.elf"

# Case 2: --no-merge-constants should keep equal constants distinct.
%eld --no-merge-constants --entry=_start "$BUILD_DIR/start.o" "$BUILD_DIR/a_const.o" "$BUILD_DIR/b_const.o" "$BUILD_DIR/a_get.o" "$BUILD_DIR/b_get.o" -o "$BUILD_DIR/merge_off.elf"
set +e
%run "$BUILD_DIR/merge_off.elf"
merge_off_rc=$?
set -e
if [[ "$merge_off_rc" -eq 0 ]]; then
  echo "FAIL: merge_off unexpectedly succeeded" >&2
  exit 1
fi

{
  echo "MERGE_ON_RC=0"
  echo "MERGE_OFF_RC=$merge_off_rc"
  echo "PASS: -fPIC merge-constants behavior verified"
} | tee "$BUILD_DIR/mergedatapic.out"
%filecheck %p/mergedatapic.golden < "$BUILD_DIR/mergedatapic.out"
#CHECK: PASS: -fPIC merge-constants behavior verified
