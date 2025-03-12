// Same as Padding.test but checks that the padding size is correct.

	.type	.L.str,%object
	.section	.rodata.str1.1,"aMS",%progbits,1
.L.str:
	.string	"foo"
	.size	.L.str, 4

	.type	foo,%object
	.section	.data.foo,"aw",%progbits
	.globl	foo
	.p2align	6, 0x0
foo:
	.long	.L.str
	.size	foo, 4

	.type	.L.str.1,%object
	.section	.rodata.str1.1,"aMS",%progbits,1
.L.str.1:
	.string	"bar"
	.size	.L.str.1, 4

	.type	bar,%object
	.section	.data.bar,"aw",%progbits
	.globl	bar
	.p2align	6, 0x0
bar:
	.long	.L.str.1
	.size	bar, 4



// RUN: %clang %s -c -o %t.o
// RUN: %link %linkopts %t.o -o %t.out -Map %t.yaml -MapStyle yaml
// RUN: %filecheck %s < %t.yaml

//      CHECK: ALIGNMENT_PADDING
// CHECK-NEXT: 0x4
// CHECK-NEXT: 0x3C
