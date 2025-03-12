
.text
.global foo
foo:
  auipc gp, %pcrel_hi(foo)
  addi gp, gp, %pcrel_lo(foo)

.section .sdata,"aw"
.global bar
bar:
.quad 1
