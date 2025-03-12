	.file	"ex.c"
	.globl	foo
	.p2align	2
	.type	foo,%function
	.code	32                      @ @foo
foo:
	.fnstart
@ BB#0:                                 @ %entry
	mov	r0, #0
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
	.cantunwind
	.fnend
