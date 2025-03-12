// ---PatchableSymbols.test--------------------------- Executable -----------------#
// BEGIN_COMMENT
// Very basic test to define patchable symbols.
// END_COMMENT
// START_TEST

// RUN: split-file %s %t
// RUN: %clang %clangopts -c -o %t/a.o %t/a.s
// RUN: %clang %clangopts -c -o %t/b.o %t/b.s
// RUN: %clang %clangopts -c -o %t/main.o %t/main.s

// RUN: %link %linkopts %t/a.o                -z now -o %t/a-base.out %t/main.o --trace=all-symbols 2>&1 | %filecheck %s --check-prefix=TRACE-BASE --match-full-lines
// RUN: %link %linkopts %t/a.o --patch-enable -z now -o %t/a.out      %t/main.o --trace=all-symbols 2>&1 | %filecheck %s --check-prefix=TRACE      --match-full-lines

// RUN: %link %linkopts %t/b.o                -z now -o %t/b-base.out %t/main.o --trace=all-symbols 2>&1 | %filecheck %s --check-prefix=TRACE-BASE --match-full-lines
// RUN: %link %linkopts %t/b.o --patch-enable -z now -o %t/b.out      %t/main.o --trace=all-symbols 2>&1 | %filecheck %s --check-prefix=TRACE      --match-full-lines

// TRACE-BASE: Trace: Object symbol created [f]: (SHNDX {{[[:digit:]]+}}) (ELF)(FUNCTION)(DEFINE)[Global]{DEFAULT}
// TRACE-BASE: Trace: Object symbol created [g]: (SHNDX {{[[:digit:]]+}}) (ELF)(FUNCTION)(DEFINE)[Global]{DEFAULT}

// TRACE: Trace: Object symbol created [f]: (SHNDX {{[[:digit:]]+}}) (ELF)(FUNCTION)(DEFINE)[Global]{DEFAULT}{PATCHABLE}
// TRACE: Trace: Object symbol created [g]: (SHNDX {{[[:digit:]]+}}) (ELF)(FUNCTION)(DEFINE)[Global]{DEFAULT}

// a.s and b.s differ by the order of the alias marker and the patchable symbol itself
//--- a.s
    .global __llvm_patchable_f
__llvm_patchable_f:

    .text

    .global f
    .type f, %function
f:  nop

    .global g
    .type g, %function
g:  nop

//--- b.s
    .text

    .global f
    .type f, %function
f:  nop

    .global g
    .type g, %function
g:  nop

    .global __llvm_patchable_f
__llvm_patchable_f:

//--- main.s
    .text

    .global _start
    .type _start, %function
main:
     .word f
     .word g
