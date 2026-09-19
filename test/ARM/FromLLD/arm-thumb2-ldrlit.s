// REQUIRES: arm
// RUN: llvm-mc --triple=thumbv7m-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: echo "SECTIONS { \
// RUN:                 .rodata.low 0x8012  : { *(.rodata.low) } \
// RUN:                 .text.low   0x8f00  : { *(.text.low) } \
// RUN:                 .text.neg   0x9000  : { *(.text.neg) } \
// RUN:                 .text.pos   0x10000 : { *(.text.pos) } \
// RUN:                 .text.high  0x10100 : { *(.text.high) } \
// RUN:                 .data_high  0x1100f : { *(.data.high) } \
// RUN:               } " > %t.script
// RUN: %link --script %t.script %t.o -o %t
// DISABLED: %readelf -s %t | %filecheck %s --check-prefix=SYMS  // TEMP DISABLED
// RUN: %readelf -x .text.neg %t | %filecheck %s --check-prefix=NEG
// RUN: %readelf -x .text.pos %t | %filecheck %s --check-prefix=POS

/// Test the various legal cases for the R_ARM_THM_PC12 relocation
/// Interesting things to note
/// Range is +- 4095 bytes
/// The Thumb bit for function symbols is ignored
/// The expression is S + A - Pa where Pa is AlignDown(PC, 4) so we will use
/// 2-byte nops to make some of the ldr instructions 2-byte aligned.
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
 .thumb_func
_start:
 nop
/// ldr r0, dat1
 .inst.w 0xf85f0004
 .reloc 2, R_ARM_THM_PC12, dat1
/// ldr r1, dat2
 .inst.w 0xf85f1004
 .reloc 6, R_ARM_THM_PC12, dat2
 nop
/// ldr r2, dat3
 .inst.w 0xf85f2004
 .reloc 0xc, R_ARM_THM_PC12, dat3
/// ldr r3, dat4
 .inst.w 0xf85f3004
 .reloc 0x10, R_ARM_THM_PC12, dat4

/// ldr r0, target1
 .inst.w 0xf85f0004
 .reloc 0x14, R_ARM_THM_PC12, target1

 nop
/// ldr r1, target2
 .inst.w 0xf85f1004
 .reloc 0x1a, R_ARM_THM_PC12, target2

 .section .text.pos, "ax", %progbits
 .balign 4
 .global pos
 .thumb_func
pos:
/// ldr r2, target3
 .inst.w 0xf85f2004
 .reloc 0, R_ARM_THM_PC12, target3
 nop
/// ldr r3, target4
 .inst.w 0xf85f3004
 .reloc 6, R_ARM_THM_PC12, target4
 nop
/// ldr r0, dat5
 .inst.w 0xf85f0004
 .reloc 0xc, R_ARM_THM_PC12, dat5
/// ldr r1, dat6
 .inst.w 0xf85f1004
 .reloc 0x10, R_ARM_THM_PC12, dat6
 nop
/// ldr r2, dat7
 .inst.w 0xf85f2004
 .reloc 0x16, R_ARM_THM_PC12, dat7
/// ldr r3, dat8
 .inst.w 0xf85f3004
 .reloc 0x1a, R_ARM_THM_PC12, dat8
/// positive addend in instruction, all others are -4 (PC bias)
///ldr.w r4, dat5 + 8
 .inst 0xf8df4004
 .reloc 0x1e, R_ARM_THM_PC12, dat5

 .section .text.high, "ax", %progbits
 .balign 4
 .thumb_func
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



/// AlignDown(0x9002+4, 4) - 0xff2 = 0x8012
/// AlignDown(0x9006+4, 4) - 0xff5 = 0x8013
/// AlignDown(0x900c+4, 4) - 0xffc = 0x8014
/// AlignDown(0x9010+4, 4) - 0xfff = 0x8015
/// AlignDown(0x9014+4, 4) - 0x118 = 0x8f00
/// AlignDown(0x901a+4, 4) - 0x11a = 0x8f02

/// AlignDown(0x10000+4, 4) + 0x1c = 0x10100
/// AlignDown(0x10006+4, 4) + 0x1a = 0x10122
/// AlignDown(0x1000c+4, 4) + 0xfff = 0x1100f
/// AlignDown(0x10010+4, 4) + 0xffc = 0x11010
/// AlignDown(0x10016+4, 4) + 0xff9 = 0x11011
/// AlignDown(0x1001a+4, 4) + 0xff6 = 0x11012
/// AlignDown(0x1001e+4, 4) + 0xff7 = 0x11017 = dat5 + 8




// NEG: 00bf5ff8 f20f5ff8 f51f00bf 5ff8fc2f
// NEG: 5ff8ff3f 5ff81801 00bf5ff8 1a11

// POS: dff8fc20 00bfdff8 fa3000bf dff8ff0f
// POS: dff8fc1f 00bfdff8 f92fdff8 f63fdff8
// POS: f74f
