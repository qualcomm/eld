	.text
	.type	foo,@object
	.section	.rodata.foo,"a",@progbits
	.weak	foo
	.align	4
foo:
	.word	10
	.size	foo, 4
	.section	.rodata.bar,"aMS",@progbits,1
	.weak	bar
	.align	4
bar:
	.word	10
	.size	bar, 4
