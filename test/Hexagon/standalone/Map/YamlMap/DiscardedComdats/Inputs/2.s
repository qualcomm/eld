	.section .text.foo,"axG",%progbits,foo_group,comdat
	.weak foo
foo:
	.word 0
