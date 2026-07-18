## Two static TLS variables via LD model, tests that DTPOFF32 relocs survive.
	.text
	.globl get_tls_pair
	.type get_tls_pair,@function
get_tls_pair:
	leaq	var1@tlsld(%rip), %rdi
	callq	__tls_get_addr@PLT
	movl	var1@dtpoff(%rax), %ecx
	movl	var2@dtpoff(%rax), %eax
	addl	%ecx, %eax
	retq

	.section .tbss,"awT",@nobits
	.globl var1
	.type var1,@object
var1:
	.zero 4
	.globl var2
	.type var2,@object
var2:
	.zero 4
