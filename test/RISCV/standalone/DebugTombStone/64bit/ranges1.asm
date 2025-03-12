	.text
	.attribute	4, 16
	.attribute	5, "rv64i2p1_m2p0_c2p0"
	.file	"ranges.c"
        .weak mygroup
mygroup:
        .quad 100
	.section	.text.foo,"axG",@progbits,mygroup,comdat
	.weak	foo
	.p2align	1
	.type	foo,@function
foo:
.Lfunc_begin0:
	li	a0, 1
.Ltmp0:
	ret
.Ltmp1:
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
