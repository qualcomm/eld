	.syntax unified
	.arch armv7-a
	.eabi_attribute 27, 3
	.eabi_attribute 28, 1
	.fpu vfpv3-d16
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.thumb
	.file	"t.c"
	.text
	.align	2
	.global	bar
	.thumb
	.thumb_func
	.type	bar, %function
bar:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	push	{r7, lr}
	add	r7, sp, #0
	b	foo
	mov	r3, r0
	mov	r0, r3
	pop	{r7, pc}
	.size	bar, .-bar
	.ident	"GCC: (crosstool-NG linaro-1.13.1-4.8-2014.01 - Linaro GCC 2013.11) 4.9.0 20140309 (experimental)"
	.section	.note.GNU-stack,"",%progbits
