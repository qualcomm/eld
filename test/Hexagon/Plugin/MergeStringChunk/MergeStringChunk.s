#---MergeStringChunk.s----------------------- Executable ---------------------------#

// START_COMMENT
// test plugin capability to read strings from merge string sections
// END_COMMENT

.section .rodata.str1,"aMS",%progbits,1
.string "one"
.string "two"
.section .rodata.str2,"aMS",%progbits,1
.string "three"
.string "four"

// RUN: %clang %clangopts -c %s -o %t.o
// RUN: %link %linkopts %t.o -T %p/Inputs/script.t -o %t.out 2>&1 | %filecheck %s

// CHECK: one 4 0 0 0 .rodata.str1
// CHECK: two 4 4 0 0 .rodata.str1
// CHECK: three 6 0 0 0 .rodata.str2
// CHECK: four 5 6 0 0 .rodata.str2
