# REQUIRES: riscv32 || riscv64

## This test is based on the lld test with the same name. The differences
## with the lld output:
## -- We don't gp-relax at the exact maximum distance for now,
## -- We don't relax the exact zero,
## -- __global_pointer$ must be defined in the linker script.
## In addition, I added test cases for zero-page boundaries.
# RUN: rm -rf %t && split-file %s %t && cd %t

# RUN: %clang %clangopts -c a.s -o %t.o

# RUN: %link %linkopts --defsym baz=420 %t.o lds -o %t.out
# RUN: %objdump -td -M no-aliases --no-show-raw-insn %t.out | FileCheck %s

# CHECK: 00000084 l       .text {{0*}}0 a

# CHECK-NOT:  lui
# CHECK:      addi    a0, gp, -0x7ff
# CHECK-NEXT: lw      a0, -0x7ff(gp)
# CHECK-NEXT: sw      a0, -0x7ff(gp)
# CHECK-NOT:  lui
# CHECK-NEXT: addi    a0, gp, 0x7fe
# CHECK-NEXT: lb      a0, 0x7fe(gp)
# CHECK-NEXT: sb      a0, 0x7fe(gp)
# CHECK-NEXT: lui     a0, 0x201
# CHECK-NEXT: addi    a0, a0, 0x0
# CHECK-NEXT: lw      a0, 0x0(a0)
# CHECK-NEXT: sw      a0, 0x0(a0)
# CHECK-NOT:  lui
# CHECK:      addi    a0, zero, 0x1
# CHECK-NEXT: lw      a0, 0x1(zero)
# CHECK-NEXT: sw      a0, 0x1(zero)
# CHECK-NOT:  lui
# CHECK:      addi    a0, zero, 0x1a4
# CHECK-NEXT: lw      a0, 0x1a4(zero)
# CHECK-NEXT: sw      a0, 0x1a4(zero)

# CHECK-NEXT: c.lui   a0, 0xfffff
# CHECK-NEXT: addi    a0, a0, 0x7ff
# CHECK-NEXT: lw      a0, 0x7ff(a0)
# CHECK-NEXT: sw      a0, 0x7ff(a0)

# CHECK-NEXT: addi    a0, zero, -0x800
# CHECK-NEXT: lw      a0, -0x800(zero)
# CHECK-NEXT: sw      a0, -0x800(zero)

# CHECK-NEXT: lui     a0, 0x0
# CHECK-NEXT: addi    a0, a0, 0x0
# CHECK-NEXT: lw      a0, 0x0(a0)
# CHECK-NEXT: sw      a0, 0x0(a0)

# CHECK-NEXT: addi    a0, zero, 0x7ff
# CHECK-NEXT: lw      a0, 0x7ff(zero)
# CHECK-NEXT: sw      a0, 0x7ff(zero)

# CHECK-NEXT: c.lui   a0, 0x1
# CHECK-NEXT: addi    a0, a0, -0x800
# CHECK-NEXT: lw      a0, -0x800(a0)
# CHECK-NEXT: sw      a0, -0x800(a0)

# CHECK-EMPTY:
# CHECK-NEXT: <a>:
# CHECK-NEXT: addi a0, a0, 0x1

#--- a.s
.global _start
_start:
  lui a0, %hi(foo)
  addi a0, a0, %lo(foo)
  lw a0, %lo(foo)(a0)
  sw a0, %lo(foo)(a0)
  lui a0, %hi(bar)
  addi a0, a0, %lo(bar)
  lb a0, %lo(bar)(a0)
  sb a0, %lo(bar)(a0)
  lui a0, %hi(norelax)
  addi a0, a0, %lo(norelax)
  lw a0, %lo(norelax)(a0)
  sw a0, %lo(norelax)(a0)
  lui a0, %hi(almost_zero)
  addi a0, a0, %lo(almost_zero)
  lw a0, %lo(almost_zero)(a0)
  sw a0, %lo(almost_zero)(a0)
  lui a0, %hi(baz)
  addi a0, a0, %lo(baz)
  lw a0, %lo(baz)(a0)
  sw a0, %lo(baz)(a0)

  lui a0, %hi(below_neg_end)
  addi a0, a0, %lo(below_neg_end)
  lw a0, %lo(below_neg_end)(a0)
  sw a0, %lo(below_neg_end)(a0)

  lui a0, %hi(neg_end)
  addi a0, a0, %lo(neg_end)
  lw a0, %lo(neg_end)(a0)
  sw a0, %lo(neg_end)(a0)

  lui a0, %hi(zero)
  addi a0, a0, %lo(zero)
  lw a0, %lo(zero)(a0)
  sw a0, %lo(zero)(a0)

  lui a0, %hi(pos_end)
  addi a0, a0, %lo(pos_end)
  lw a0, %lo(pos_end)(a0)
  sw a0, %lo(pos_end)(a0)

  lui a0, %hi(past_pos_end)
  addi a0, a0, %lo(past_pos_end)
  lw a0, %lo(past_pos_end)(a0)
  sw a0, %lo(past_pos_end)(a0)
a:
  .option norvc
  addi a0, a0, 1

.section .sdata,"aw"
  .space 1
foo:
  .word 0
  .space 4089
bar:
  .byte 0
  .space 1
norelax:
  .word 0

#--- lds
SECTIONS {
  .text : {*(.text) }
  .sdata 0x200000 : {
    __global_pointer$ = . + 0x800;
  }
  below_neg_end = -(0x800 + 1);
  neg_end = -0x800;
  zero = 0;
  almost_zero = 1;
  pos_end = 0x800 - 1;
  past_pos_end = 0x800;
}
