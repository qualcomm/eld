	.text
	.file	"lto.c"
	.globl	baz
	.falign
	.type	baz,@function
baz:                                    // @baz
// BB#0:                                // %entry
	{
		r0 = #0
		jumpr r31
	}
.Lfunc_end0:
.Ltmp0:
	.size	baz, .Ltmp0-baz
