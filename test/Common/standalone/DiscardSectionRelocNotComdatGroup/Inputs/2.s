.section .text
.long foo
.section .foo, "awG", %progbits, foo
foo:
.long 0
