	.text
	.globl get_tls
get_tls:
	leaq	tls_var@tlsld(%rip), %rdi
	callq	__tls_get_addr@PLT
	movl	tls_var@dtpoff(%rax), %eax
	retq

	.section .tbss,"awT",@nobits
	.globl tls_var
tls_var:
	.zero 4
