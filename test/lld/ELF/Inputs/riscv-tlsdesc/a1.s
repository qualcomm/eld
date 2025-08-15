.macro load dst, src
.ifdef ELF32
lw \dst, \src
.else
ld \dst, \src
.endif
.endm

.Ltlsdesc_hi2:
  auipc a4, %tlsdesc_hi(c)
  load  a6, %tlsdesc_load_lo(.Ltlsdesc_hi2)(a4)
  addi  a0, a4, %tlsdesc_add_lo(.Ltlsdesc_hi2)
  jalr  t0, 0(a6), %tlsdesc_call(.Ltlsdesc_hi2)
  add   a0, a0, tp
