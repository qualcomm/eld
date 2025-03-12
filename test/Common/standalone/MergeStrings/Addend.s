// ---Addend.s--------------------------- Executable -----------------#

// BEGIN_COMMENT
// Tests a bug with merge strings where a relocation (.L.str + 5) was
// assigned the wrong target fragmentref ("bar" instead of "foo") and that
// fragment ("bar") was merged.
// END_COMMENT
.section .rodata.str1.1,"aMS",%progbits,1
.string "abc"
.string "bar"
.string "baz"
.text
.section	.rodata.str1.1,"aMS",%progbits,1
.L.str:
	.string	"foo"
	.size	.L.str, 4

	.type	foo,%object
	.data
	.globl	foo
	.p2align	2, 0x0
foo:
	.word	.L.str + 5
	.size	foo, 4

	.type	.L.str.1,%object
	.section	.rodata.str1.1,"aMS",%progbits,1
.L.str.1:
	.string	"bar"
	.size	.L.str.1, 4

	.type	bar,%object
	.data
	.globl	bar
	.p2align	2, 0x0
bar:
	.word	.L.str.1 + 0
	.size	bar, 4

// START_TEST
// RUN: %clang %clangopts -c %s -o %t.o
// RUN: %link %linkopts --emit-relocs %t.o --trace=merge-strings=all -T %p/Inputs/addend.t -o %t.out
// RUN: %readelf -s %t.out | %filecheck %s --check-prefix=SYM
// RUN: %objdump -D %t.out | %filecheck %s --check-prefix=DUMP
// END_TEST

// SYM:  100c {{.*}} .L.str
// DUMP: 1010 <foo>:
// ARM has a different dissassembly format
// DUMP: 1010: {{11 10|1011|00001011}}

