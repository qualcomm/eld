// UNSUPPORTED: riscv32, riscv64
//---StringOffsets.test----------------------- Executable -----------------#

// BEGIN_COMMENT
// Checks that string offsets/sizes are correct in the txt map file

.section .rodata.str1.1,"aMS",%progbits,1
.string	"abc"
.section .rodata.str2.1,"aMS",%progbits,1
.string "foo"
.string "abc"
.string "bar"
.string "baz"

// START_TEST
// RUN: %clang %clangopts -c %s -o %t.o
// RUN: %link %linkopts %t.o -o %t.out -Map %t.map
// RUN: %filecheck %s < %t.map
// END_TEST

// CHECK: .rodata.str2.1	0x4	0x4
// CHECK: .rodata.str2.1	0x8	0x4
// CHECK: .rodata.str2.1	0xc	0x4
