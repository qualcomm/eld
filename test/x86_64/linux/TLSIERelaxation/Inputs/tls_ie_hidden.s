	.globl tls_hidden
	.hidden tls_hidden
	.section .tbss,"awT",@nobits
	.align 4
tls_hidden:
	.long 0

	.text
	.globl get_tls
get_tls:
	movq	tls_hidden@GOTTPOFF(%rip), %rax
	ret
