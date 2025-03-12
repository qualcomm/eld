	.text
	.globl	foo                     // -- Begin function foo
	.p2align	3
	.type	foo,@function
foo:                                    // @foo
// %bb.0:                               // %entry
	mov	w0, #0
        adrp    x23, .L.str.41+1
        add     x23, x23, :lo12:.L.str.41+1
	ret
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
                                        // -- End function
        .type   .L.str.40,@object
        .section        .rodata.str1.1,"aMS",@progbits,1
.L.str.40:
        .asciz  "graphics"
        .size   .L.str.40, 9

        .type   .L.str.41,@object
.L.str.41:
        .asciz  "graphics"
        .size   .L.str.41, 9
