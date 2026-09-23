// REQUIRES: arm
// RUN: llvm-mc --triple=armv7a-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: echo "SECTIONS { \
// RUN:                 .rodata.low 0x8012  : { *(.rodata.low) } \
// RUN:                 .text.low   0x8f00  : { *(.text.low) } \
// RUN:                 .text.neg   0x9000  : { *(.text.neg) } \
// RUN:                 .text.pos   0x10000 : { *(.text.pos) } \
// RUN:                 .text.high  0x10100 : { *(.text.high) } \
// RUN:                 .data_high  0x1100f : { *(.data.high) } \
// RUN:               } " > %t.script
// RUN: %link -n --script %t.script %t.o -o %t
// RUN: %readelf --symbols %t | %filecheck %s --check-prefix=SYMS
// RUN: %readelf -x .text.neg %t | %filecheck %s --check-prefix=NEG
// RUN: %readelf -x .text.pos %t | %filecheck %s --check-prefix=POS

/// Test the various legal cases for the R_ARM_LDR_PC_G0 relocation
/// Range is +- 4095 bytes
/// The Thumb bit for function symbols is ignored
 .section .rodata.low, "a", %progbits
dat1:
 .byte 0
dat2:
 .byte 1
dat3:
 .byte 2
dat4:
 .byte 3

 .section .text.low, "ax", %progbits
 .balign 4
 .global target1
 .type target1, %function
target1:
 bx lr
 .type target2, %function
target2:
 bx lr

 .section .text.neg, "ax", %progbits
 .balign 4
 .global _start
 .type _start, %function
_start:
/// ldr r0, dat1  [pc, #-4086]
 .inst 0xe51f0008
 .reloc 0, R_ARM_LDR_PC_G0, dat1
/// ldr r1, dat2  [pc, #-4089]
 .inst 0xe51f1008
 .reloc 4, R_ARM_LDR_PC_G0, dat2
/// ldr r2, dat3  [pc, #-4092]
 .inst 0xe51f2008
 .reloc 8, R_ARM_LDR_PC_G0, dat3
/// ldr r3, dat4  [pc, #-4095]
 .inst 0xe51f3008
 .reloc 0xc, R_ARM_LDR_PC_G0, dat4
/// ldr r0, target1  [pc, #-280]
 .inst 0xe51f0008
 .reloc 0x10, R_ARM_LDR_PC_G0, target1
/// ldr r1, target2  [pc, #-280]
 .inst 0xe51f1008
 .reloc 0x14, R_ARM_LDR_PC_G0, target2

 .section .text.pos, "ax", %progbits
 .balign 4
 .global pos
 .type pos, %function
pos:
/// ldr r2, target3  [pc, #248]
 .inst 0xe51f2008
 .reloc 0, R_ARM_LDR_PC_G0, target3
/// ldr r3, target4  [pc, #248]
 .inst 0xe51f3008
 .reloc 4, R_ARM_LDR_PC_G0, target4
/// ldr r0, dat5  [pc, #4095]
 .inst 0xe51f0008
 .reloc 8, R_ARM_LDR_PC_G0, dat5
/// ldr r1, dat6  [pc, #4092]
 .inst 0xe51f1008
 .reloc 0xc, R_ARM_LDR_PC_G0, dat6
/// ldr r2, dat7  [pc, #4089]
 .inst 0xe51f2008
 .reloc 0x10, R_ARM_LDR_PC_G0, dat7
/// ldr r3, dat8  [pc, #4086]
 .inst 0xe51f3008
 .reloc 0x14, R_ARM_LDR_PC_G0, dat8
/// ldr r4, dat5+8  [pc, #4087]
 .inst 0xe59f4000
 .reloc 0x18, R_ARM_LDR_PC_G0, dat5

 .section .text.high, "ax", %progbits
 .balign 4
 .type target3, %function
 .global target3
target3:
 bx lr
 .thumb_func
target4:
 bx lr

 .section .data.high, "aw", %progbits
dat5:
 .byte 0
dat6:
 .byte 1
dat7:
 .byte 2
dat8:
 .byte 3

// SYMS: 00008012 {{.*}} dat1
// SYMS: 00008013 {{.*}} dat2
// SYMS: 00008014 {{.*}} dat3
// SYMS: 00008015 {{.*}} dat4
// SYMS: 0001100f {{.*}} dat5
// SYMS: 00011010 {{.*}} dat6
// SYMS: 00011011 {{.*}} dat7
// SYMS: 00011012 {{.*}} dat8

/// Negative offsets: U=0, ldr r0 [pc, #-4086], r1 [pc, #-4089] etc
// NEG: f60f1fe5 f91f1fe5 fc2f1fe5 ff3f1fe5
// NEG: 18011fe5 18111fe5

/// Positive offsets: U=1
// POS: f8209fe5 f8309fe5 ff0f9fe5 fc1f9fe5
// POS: f92f9fe5 f63f9fe5 f74f9fe5
