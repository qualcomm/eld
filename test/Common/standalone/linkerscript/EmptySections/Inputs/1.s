.section .text.bar
.p2align 2
.data
.p2align 4
.bss
.p2align 8
.section .text.foo
.global foo
foo:
.word .bss
