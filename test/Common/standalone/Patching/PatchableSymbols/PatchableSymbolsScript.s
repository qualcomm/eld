// ---PatchableSymbols.test--------------------------- Executable -----------------#

// BEGIN_COMMENT
// Check for error with patchable symbol also defined in linker script.
// END_COMMENT
// START_TEST

// RUN: %clang %clangopts -c -o %t.o %s
// RUN: %not %link %linkopts %t.o --patch-enable -o %t.out -t %p/Inputs/script.t 2>&1 | %filecheck %s

    .text

    .global f
    .type f, %function
f:  .word f

    .global __llvm_patchable_f
__llvm_patchable_f:

// CHECK: Error: Patchable symbol 'f' is overridden by linker script
