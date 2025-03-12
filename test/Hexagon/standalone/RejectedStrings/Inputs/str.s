	.text
	.file	"str.c"
	.type	.L.str,@object          // @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.string	"foo"
	.size	.L.str, 4

	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str1:
	.string	"bar"
	.size	.L.str1, 4
	.section	.rodata.str1.1,"aMS",@progbits,1
        .align 8
.L.str2:
	.string	"baz"
	.size	.L.str2, 4
