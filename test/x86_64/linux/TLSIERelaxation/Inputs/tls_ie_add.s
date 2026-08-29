	.globl tls_var
	.section .tbss,"awT",@nobits
	.align 4
tls_var:
	.long 0

	.text
	.globl get_tls
get_tls:
	addq	tls_var@GOTTPOFF(%rip), %rax
	ret
