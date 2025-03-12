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
        .space  0xb000
	.size	bar, .-bar
	.ident	"GCC: (crosstool-NG linaro-1.13.1-4.8-2014.01 - Linaro GCC 2013.11) 4.9.0 20140309 (experimental)"
        .section        .data.bar,"ax",%progbits
        .align  15
        .space  0xb000
        .section        .foo,"ax",%progbits
        .align  2
        .word   0x0
	.section	.note.GNU-stack,"",%progbits
