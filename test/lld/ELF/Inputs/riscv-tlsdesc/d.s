.macro load dst, src
.ifdef ELF32
lw \dst, \src
.else
ld \dst, \src
.endif
.endm

.Ltlsdesc_hi0:
  auipc	a0, %tlsdesc_hi(foo)
  load	a1, %tlsdesc_load_lo(.Ltlsdesc_hi0)(a0)
  addi	a0, a0, %tlsdesc_add_lo(.Ltlsdesc_hi0)
  jalr	t0, 0(a1), %tlsdesc_call(.Ltlsdesc_hi0)
  add	a1, a0, tp
