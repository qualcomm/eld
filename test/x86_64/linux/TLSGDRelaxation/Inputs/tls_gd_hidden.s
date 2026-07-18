	.globl tls_hidden
	.hidden tls_hidden
	.section .tbss,"awT",@nobits
tls_hidden:
	.zero 4

	.text
	.globl get_tls
get_tls:
	.byte 0x66
	leaq tls_hidden@tlsgd(%rip), %rdi
	.byte 0x66, 0x66, 0x48
	call __tls_get_addr@PLT
	retq
