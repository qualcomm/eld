// Tests that R_ARM_THM_JUMP19 (conditional branch) produces both T2T and T2A veneers
// when the targets are out of range (beyond 21-bit range).
// Regression test for https://github.com/qualcomm/eld/issues/1005

// RUN: %clang %clangopts %s -o %t.o -c --target=arm-none-eabi -march=armv7-a -mfloat-abi=soft
// RUN: echo "SECTIONS { .text.caller 0x0 : { *(.text.caller) } .text.thumb_target 0x200000 : { *(.text.thumb_target) } .text.arm_target 0x400000 : { *(.text.arm_target) } }" > %t.script
// RUN: %link %linkopts -T %t.script %t.o -o %t.out --entry=my_func -static --trace=trampolines 2>&1 | %filecheck %s

// CHECK: Creating Stub __thumb_target_{{.*}}veneer@island-{{.*}}
// CHECK: Creating Stub __arm_target_{{.*}}veneer@island-{{.*}}

.syntax unified

// --- CALLER (Thumb) ---
.section .text.caller, "ax", %progbits
.thumb
.thumb_func
.global my_func
.type   my_func, %function
my_func:
    cmp  r0, #0
    beq  thumb_target   // Triggers T2T Veneer
    beq  arm_target     // Triggers T2A Veneer
    bx   lr

// --- TARGET 1 (Thumb) ---
.section .text.thumb_target, "ax", %progbits
.thumb
.thumb_func
.global thumb_target
.type thumb_target, %function
thumb_target:
    bx   lr

// --- TARGET 2 (ARM) ---
.section .text.arm_target, "ax", %progbits
.arm
.global arm_target
.type arm_target, %function
arm_target:
    bx   lr