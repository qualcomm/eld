#---GlobalMergeNonAllocStrings.test----------------------- Executable -----------------#


// START_COMMENT
// tests that non alloc merge strings can be merged across output sections
// when --global-merge-non-alloc-strings option is specified
// END_COMMENT

.section .debug_a, "MS",%progbits,1
.string "a"
.string "b"
.string "c"

.section .debug_b, "MS",%progbits,1
.string "a"
.string "b"
.string "c"
.string "d"
.string "e"
.string "f"

// START_TEST
// RUN: %clang %clangopts -c %s -o %t.o
// RUN: %link %linkopts %t.o --global-merge-non-alloc-strings -o %t.out
// RUN: %readelf -p .debug_a %t.out | %filecheck %s --check-prefix=A
// RUN: %readelf -p .debug_b %t.out | %filecheck %s --check-prefix=B
// END_TEST

//A: String dump of section '.debug_a':
//A: [     0] a
//A: [     2] b
//A: [     4] c

//B: String dump of section '.debug_b':
//B: [     0] d
//B: [     2] e
//B: [     4] f
