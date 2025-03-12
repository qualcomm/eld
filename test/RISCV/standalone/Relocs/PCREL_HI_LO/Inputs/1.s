.section .text.foo
.globl foo
.type foo, @function
foo:
1: auipc a1, %pcrel_hi(bar)
   addi a1, a1, %pcrel_lo(1b)
   jalr a1
2: j 2b

.section .text.bar
.globl bar
.type bar, @function
bar:
  nop
