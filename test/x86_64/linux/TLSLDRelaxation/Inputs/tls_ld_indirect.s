## TLSLD with indirect GOT call: leaq foo@tlsld(%rip),%rdi + call *__tls_get_addr@GOTPCREL(%rip)
## The GOTPCRELX reloc on the call is the indirect-call LD sequence variant.
	.text
	.globl get_tls
	.type get_tls,@function
get_tls:
	leaq	tls_var@tlsld(%rip), %rdi
	call	*__tls_get_addr@GOTPCREL(%rip)
	movl	tls_var@dtpoff(%rax), %eax
	retq

	.section .tbss,"awT",@nobits
	.globl tls_var
	.type tls_var,@object
tls_var:
	.zero 4
