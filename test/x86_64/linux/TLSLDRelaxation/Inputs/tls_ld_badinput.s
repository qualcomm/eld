## Manually placed TLSLD reloc at offset 0 via .reloc: the write at
## out_off-3 would be before the section start. The linker must not crash.
## The reloc guard (offset >= 3) must reject this candidate without asserting.
	.text
	.globl bad_fn
	.type bad_fn,@function
bad_fn:
	.reloc ., R_X86_64_TLSLD, tls_var
	.long 0
	retq

	.section .tbss,"awT",@nobits
	.globl tls_var
	.type tls_var,@object
tls_var:
	.zero 4
