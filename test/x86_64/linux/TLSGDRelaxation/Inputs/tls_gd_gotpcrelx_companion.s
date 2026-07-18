	.globl tls_extern_def
	.section .tbss,"awT",@nobits
tls_extern_def:
	.zero 4

	.text
	.globl get_tls_noplt
get_tls_noplt:
	.byte 0x66
	leaq tls_extern_def@tlsgd(%rip), %rdi
	.byte 0x66, 0x48
	call *__tls_get_addr@GOTPCREL(%rip)
	retq
