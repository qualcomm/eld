## Two LD-model TLS variables: tests that DTPOFF32 relocs produce
## TP-relative (negative) offsets after LD→LE relaxation, not
## module-relative (positive) dtpoff values.
## .tdata comes first (var1 at offset 0, var2 at offset 4 within TLS block),
## so TPOFF values will be negative on x86-64 variant-2.
	.text
	.globl get_sum
	.type get_sum,@function
get_sum:
	leaq	var1@tlsld(%rip), %rdi
	callq	__tls_get_addr@PLT
	movl	var1@dtpoff(%rax), %ecx
	movl	var2@dtpoff(%rax), %edx
	addl	%ecx, %edx
	movl	%edx, %eax
	retq

	.section .tdata,"awT",@progbits
	.globl var1
	.type var1,@object
var1:
	.long 0
	.globl var2
	.type var2,@object
var2:
	.long 0
