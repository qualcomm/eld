// UNSUPPORTED: arm, aarch64, riscv32, riscv64
#---RevisionInfo.s----------------------- Executable ---------------------------#

// START_COMMENT
// check for llvm and eld revision info in map file
// END_COMMENT

nop

// START_TEST
// RUN: %clang %clangopts -c %s -o %t.o
// RUN: %link %linkopts %t.o -M -o %t.out 2>&1 | %filecheck %s
// END_TEST

// CHECK: Linker: {{[0-9a-f]+}}
// CHECK: LLVM: {{[0-9a-f]+}}
