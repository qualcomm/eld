#---RelocatableGC.s----------------------- Executable ---------------------------#

// START_COMMENT
// check that a warning is emitted when mixing -r and --gc-sections
// END_COMMENT

nop

// START_TEST
// RUN: %clang %clangopts -c %s -o %t.o
// RUN: %link %linkopts %t.o -r --gc-sections -o %t.out 2>&1 | %filecheck %s
// END_TEST

// CHECK: --gc-sections has no effect when building a relocatable file