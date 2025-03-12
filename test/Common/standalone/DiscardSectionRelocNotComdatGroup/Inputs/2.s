.section .text
.word foo
.section .foo, "awG", %progbits, foo
foo:
.word 0
