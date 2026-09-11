	.globl tls_var
	.section .tbss,"awT",@nobits
tls_var:
	.zero 4

	.text
	.globl get_tls
get_tls:
	leaq	tls_var@tlsld(%rip), %rdi
	callq	__tls_get_addr@PLT
	movl	tls_var@dtpoff(%rax), %eax
	retq
