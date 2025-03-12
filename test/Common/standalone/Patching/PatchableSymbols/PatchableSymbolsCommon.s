// ---PatchableSymbols.test--------------------------- Executable -----------------#

// BEGIN_COMMENT
// Check for error with common patchable symbol.
// END_COMMENT
// START_TEST

// RUN: %clang %clangopts -c -o %t.o %s
// RUN: %not %link %linkopts %t.o --patch-enable -o %t.out 2>&1 | %filecheck %s

    .text
    .comm f,4
    .type f, %function

    .global __llvm_patchable_f
__llvm_patchable_f:

// CHECK: Error: Symbol 'f' from file '{{.*}}PatchableSymbolsCommon.s.tmp.o' cannot be patched. Only global non-common symbols can be patched
