	.section .text.foo,"axG",%progbits,foo_group,comdat
        .local bar
bar:
        .word 0
        .local baz
baz:
        .word 0
	.weak foo
foo:
	.word 0
        .weak foo_group
foo_group:
        .word 0

