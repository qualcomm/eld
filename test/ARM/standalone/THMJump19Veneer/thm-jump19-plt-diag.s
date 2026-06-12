// Tests that R_ARM_THM_JUMP19 correctly emits a detailed error 
// when attempting to branch to a PLT entry.
// Regression test for https://github.com/qualcomm/eld/issues/1005

// RUN: %clang %clangopts %s -o %t.o -c --target=arm-none-eabi -march=armv7-a
// RUN: not %link %linkopts -shared %t.o -o %t.so 2>&1 | %filecheck %s

// CHECK: Error:
// CHECK-SAME: conditional branch to PLT in THUMB-2 not supported yet for symbol `extern_func'
// CHECK-SAME: [.text.caller][.text.caller]

    .syntax unified
    .thumb

    .section .text.caller, "ax", %progbits
    .thumb_func
    .global my_func
    .type   my_func, %function
my_func:
    cmp  r0, #0
    beq  extern_func  // Conditional branch to an undefined external symbol
    bx   lr
