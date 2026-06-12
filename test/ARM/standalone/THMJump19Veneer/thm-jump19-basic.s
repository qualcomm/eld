// Tests that R_ARM_THM_JUMP19 (conditional branch) links successfully
// for an in-range target without requiring a stub.
// Regression test for https://github.com/qualcomm/eld/issues/1005

// RUN: %clang %clangopts %s -o %t.o -c --target=arm-none-eabi -mcpu=cortex-m33 -mfloat-abi=soft
// RUN: %link %linkopts %t.o -o %t.out --entry=my_func -static
// RUN: %objdump -d %t.out | %filecheck %s

// CHECK: <my_func>:
// CHECK:      {{.*}} beq.w   {{.*}} <plain_entry>

// CHECK: <plain_entry>:
// CHECK-NEXT: {{.*}} bx      lr

    .syntax unified
    .thumb

    .section .text.caller, "ax", %progbits
    .thumb_func
    .global my_func
    .type   my_func, %function
my_func:
    cmp  r0, #0
    beq  plain_entry
    bx   lr

    .section .text.target, "ax", %progbits
    .global plain_entry
plain_entry:
    bx   lr