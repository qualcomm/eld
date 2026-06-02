## Single HI20 reference to global_var. Linker script places .data at an
## address whose HI20 overflows the 32-bit signed range; the resulting
## R_RISCV_HI20 overflow diagnostic is fully deterministic.
.section .text.main,"ax",@progbits
.globl main
main:
  lui a0, %hi(global_var)
.size main, .-main
.data
.globl global_var
global_var: .word 11
