.section .text
.word foo

.section .foo, "awG", %progbits, foo, comdat
.weak foo
.hidden foo
foo:
.word 0
