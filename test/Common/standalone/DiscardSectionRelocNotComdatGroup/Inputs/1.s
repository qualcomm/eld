.section .text
.long foo

.section .foo, "awG", %progbits, foo, comdat
.weak foo
.hidden foo
foo:
.long 0
