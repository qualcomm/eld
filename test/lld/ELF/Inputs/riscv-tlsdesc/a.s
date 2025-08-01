.macro load dst, src
.ifdef ELF32
lw \dst, \src
.else
ld \dst, \src
.endif
.endm

.Ltlsdesc_hi0:
  auipc a0, %tlsdesc_hi(a)
  load  a1, %tlsdesc_load_lo(.Ltlsdesc_hi0)(a0)
  addi  a0, a0, %tlsdesc_add_lo(.Ltlsdesc_hi0)
  jalr  t0, 0(a1), %tlsdesc_call(.Ltlsdesc_hi0)
  add   a0, a0, tp

.Ltlsdesc_hi1:
  auipc a2, %tlsdesc_hi(b)
  load  a3, %tlsdesc_load_lo(.Ltlsdesc_hi1)(a2)
  addi  a0, a2, %tlsdesc_add_lo(.Ltlsdesc_hi1)
  jalr  t0, 0(a3), %tlsdesc_call(.Ltlsdesc_hi1)
  add   a0, a0, tp

.Ltlsdesc_hi2:
  auipc a4, %tlsdesc_hi(c)
  load  a5, %tlsdesc_load_lo(.Ltlsdesc_hi2)(a4)
  addi  a0, a4, %tlsdesc_add_lo(.Ltlsdesc_hi2)
  jalr  t0, 0(a5), %tlsdesc_call(.Ltlsdesc_hi2)
  add   a0, a0, tp

.section .tbss
.globl a
.zero 8
a:
.zero 2039  ## Place b at 0x7ff
b:
.zero 1
