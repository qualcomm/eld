	.text
	.type	foo,%object
	.section	.rodata.foo,"aG",%progbits,foo,comdat
	.weak	foo
	.align	4
foo:
	.word	10
	.size	foo, 4
	.section	.rodata.bar,"aG",%progbits,bar,comdat
	.weak	bar
	.align	4
bar:
	.word	10
	.size	bar, 4
