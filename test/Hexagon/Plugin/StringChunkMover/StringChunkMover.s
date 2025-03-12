#---StringChunkMover.s----------------------- Executable ---------------------------#

// START_COMMENT
// test that merge string chunks chan be moved
// END_COMMENT

.section .rodata.str1.1,"aMS",%progbits,1
.string "one"
.string "two"
.section .rodata.str1.2,"aMS",%progbits,1
.string "three"
.string "four"
.section .text

// RUN: %clang %clangopts -c %s -o %t.o
// RUN: %link %linkopts -T %p/Inputs/script.t %t.o -o %t.out
// RUN: %readelf -p .text -p .rodata %t.out 2>&1 | %filecheck %s

// CHECK: could not find section '.rodata'
// CHECK: one
// CHECK-NEXT: two
// CHECK-NEXT: three
// CHECK-NEXT: four
