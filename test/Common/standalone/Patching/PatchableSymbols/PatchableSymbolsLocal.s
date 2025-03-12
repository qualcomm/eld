// ---PatchableSymbols.test--------------------------- Executable -----------------#

// BEGIN_COMMENT
// Check for error with local patchable symbol.
// END_COMMENT
// START_TEST

// RUN: %clang %clangopts -c -o %t.o %s
// RUN: %not %link %linkopts %t.o --patch-enable -o %t.out 2>&1 | %filecheck %s

    .text
    .type f, %function
    .global __llvm_patchable_f
f:
__llvm_patchable_f:

// CHECK: Error: Symbol 'f' from file '{{.*}}PatchableSymbolsLocal.s.tmp.o' cannot be patched. Only global non-common symbols can be patched
