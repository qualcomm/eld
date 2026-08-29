	.globl tls_default
	.section .tbss,"awT",@nobits
tls_default:
	.zero 4

	.text
	.globl get_tls_preempt
get_tls_preempt:
	.byte 0x66
	leaq tls_default@tlsgd(%rip), %rdi
	.byte 0x66, 0x66, 0x48
	call __tls_get_addr@PLT
	retq
