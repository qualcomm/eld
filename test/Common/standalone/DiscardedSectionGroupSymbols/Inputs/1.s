	.section .text.foo,"axG",%progbits,foo_group,comdat
	.weak foo
foo:
	.word 0
        .weak foo_group
foo_group:
        .word 0
