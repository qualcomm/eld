## A TLSLD relocation whose fragment is truncated: the leaq opcode and 4-byte
## displacement are present (offset 3), but the following call instruction is
## missing — the section ends right after the displacement. relaxTLSLDReloc
## reads loc[4]/loc[5] to detect the direct vs indirect call variant, so the
## record-time guard (offset + 6 <= region size) must reject this candidate
## to avoid an out-of-bounds read past the fragment. The linker must not crash.
	.text
	.globl trunc_fn
	.type trunc_fn,@function
trunc_fn:
	.byte 0x48, 0x8d, 0x3d          # leaq opcode + ModR/M (3 bytes)
	.reloc ., R_X86_64_TLSLD, tls_var  # TLSLD at offset 3 (the displacement)
	.long 0                          # 4-byte disp; section ends here (7 bytes)

	.section .tbss,"awT",@nobits
	.globl tls_var
	.hidden tls_var
	.type tls_var,@object
tls_var:
	.zero 4
