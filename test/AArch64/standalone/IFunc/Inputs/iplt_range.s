// Reference the linker-defined IRELATIVE range symbols (as glibc's crt does)
// so that they are defined in a static executable. Only linked into the static
// STANDALONE case.
	.data
	.globl	iplt_range
iplt_range:
	.xword	__rela_iplt_start
	.xword	__rela_iplt_end
