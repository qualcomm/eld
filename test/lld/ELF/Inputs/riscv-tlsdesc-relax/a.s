.macro load dst, src
.ifdef ELF32
lw \dst, \src
.else
ld \dst, \src
.endif
.endm

.globl _start
_start:
  .word 0

foo:
.Ltlsdesc_hi0:
.option norelax
## All 4 instructions have an R_RISCV_RELAX.
## Check that optimization/relaxation are not affected by irrelevant instructions.
  auipc a2, %tlsdesc_hi(b)
  .reloc .-4, R_RISCV_RELAX, 0
  c.add a7, a7
  load  a3, %tlsdesc_load_lo(.Ltlsdesc_hi0)(a2)
  .reloc .-4, R_RISCV_RELAX, 0
  c.add a7, a7
  addi  a0, a2, %tlsdesc_add_lo(.Ltlsdesc_hi0)
  .reloc .-4, R_RISCV_RELAX, 0
  c.add a7, a7
  jalr  t0, 0(a3), %tlsdesc_call(.Ltlsdesc_hi0)
  .reloc .-4, R_RISCV_RELAX, 0
  add   a0, a0, tp
.option relax

  .word 0

.Ltlsdesc_hi1:
.option norelax
## AUIPC has an R_RISCV_RELAX. We perform relaxation, ignoring whether other
## instructions have R_RISCV_RELAX.
  auipc a4, %tlsdesc_hi(b)
  .reloc .-4, R_RISCV_RELAX, 0
  load  a5, %tlsdesc_load_lo(.Ltlsdesc_hi1)(a4)
  addi  a0, a4, %tlsdesc_add_lo(.Ltlsdesc_hi1)
  jalr  t0, 0(a5), %tlsdesc_call(.Ltlsdesc_hi1)
  add   a0, a0, tp
.option relax

.Ltlsdesc_hi2:
.option norelax
## AUIPC does not have R_RISCV_RELAX. No relaxation.
  auipc a6, %tlsdesc_hi(b)
  load  a7, %tlsdesc_load_lo(.Ltlsdesc_hi2)(a6)
  .reloc .-4, R_RISCV_RELAX, 0
  addi  a0, a6, %tlsdesc_add_lo(.Ltlsdesc_hi2)
  .reloc .-4, R_RISCV_RELAX, 0
  jalr  t0, 0(a7), %tlsdesc_call(.Ltlsdesc_hi2)
  add   a0, a0, tp
.option relax

.section .tbss
.globl a
.zero 8
a:
.zero 2039+PAD  ## Place b at 0x7ff+PAD

