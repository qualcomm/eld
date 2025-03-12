.section .dontignoreme, "aw", @progbits
.global goo
goo:
nop
.section .text.foo, "ax", @progbits
.p2align 2
r0=##goo
nop
.section .text.bar, "ax", @progbits
.p2align 2
nop
