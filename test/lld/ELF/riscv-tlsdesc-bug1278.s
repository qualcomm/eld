# REQUIRES: riscv32 || riscv64

## A regression test for a bug when incorrect R_RISCV_RELAX relocation was used
## to determine if a relocation is relaxable, due to shifting offsets.

# RUN: %llvm-mc -filetype=obj -mattr=+c,+relax %s -o %t.o
# RUN: %link %linkopts -T %p/Inputs/riscv-tlsdesc-bug1278/script.t %t.o -o %t.out
# RUN: %objdump --no-show-raw-insn -M no-aliases -h -d %t.out | FileCheck %s --check-prefix=CHECK

.text

# CHECK-LABEL: foo_1
# CHECK-NEXT:      addi zero, zero, 0x0
# CHECK-NEXT:      addi zero, zero, 0x0
# CHECK-NEXT:      addi zero, zero, 0x0
# CHECK-NEXT:      addi a0, zero, 0x0

foo_1:
.option relax
  lui a7, %hi(a)
.option norelax
.Ltlsdesc_hi_1:
  auipc a4, %tlsdesc_hi(b)
  lw a5, %tlsdesc_load_lo(.Ltlsdesc_hi_1)(a4)
  addi a0, a4, %tlsdesc_add_lo(.Ltlsdesc_hi_1)
  jalr t0, 0(a5), %tlsdesc_call(.Ltlsdesc_hi_1)

# CHECK-LABEL: foo_2
# CHECK-NEXT:      addi zero, zero, 0x0
# CHECK-NEXT:      addi zero, zero, 0x0
# CHECK-NEXT:      addi zero, zero, 0x0
# CHECK-NEXT:      addi a0, zero, 0x0

foo_2:
.option relax
  lui a7, %hi(a)
.option norelax
.Ltlsdesc_hi_2:
  auipc a4, %tlsdesc_hi(b)
.option relax
  lw a5, %tlsdesc_load_lo(.Ltlsdesc_hi_2)(a4)
  addi a0, a4, %tlsdesc_add_lo(.Ltlsdesc_hi_2)
  jalr t0, 0(a5), %tlsdesc_call(.Ltlsdesc_hi_2)


.data
.globl a
a:
  .word 0

.section .tbss
.tbss
.globl b
b:
  .word 0
