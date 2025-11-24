// RUN: llvm-mc -filetype=obj -triple=i686-pc-linux %s -o %t.o
// RUN: ld.lld --gc-sections %t.o -o %t
// RUN: llvm-readobj -s %t | %filecheck %s

// Test that the we don't gc the personality function.
// CHECK: Name: .foobar

	.globl	_start
_start:
	.cfi_startproc
	.cfi_personality 3, foobar
	.cfi_endproc
        .section .text.foobar,"ax"
foobar:
