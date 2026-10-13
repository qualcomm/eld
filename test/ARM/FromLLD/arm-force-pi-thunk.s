// REQUIRES: arm
// RUN: split-file %s %t
// RUN: llvm-mc -arm-add-build-attributes -filetype=obj -triple=armv7a-none-linux-gnueabi %t/asm -o %t.o
// RUN: %link --script %t/lds %t.o -o %t.abs.out
// RUN: %objdump -s -j .text_low -j .text_high %t.abs.out | %filecheck %s -check-prefix=STATIC
// RUN: %link --pic-veneer --script %t/lds %t.o -o %t.pic.out
// RUN: %objdump -s -j .text_low -j .text_high %t.pic.out | %filecheck %s -check-prefix=PICVENEER

// Ported from lld's arm-force-pi-thunk.s: test that we can force generation
// of position independent Thumb-to-Thumb long-branch veneers even when
// inputs are not PIC. Uses raw byte dumps (objdump -s) rather than
// disassembly (objdump -d) because a pre-existing, unrelated bug in
// THMToTHMStub's mapping-symbol placement corrupts mode-aware disassembly
// for this stub on application targets (see
// ../standalone/MappingSymbolsForVeneers/T2TApplication.test); the
// underlying instruction bytes are still verified exactly.

//--- lds

SECTIONS {
  .text_low 0x94 : AT(0x94) { *(.text_low) *(.text_low2) }
  .text_high 0x2000000 : AT(0x2000000) { *(.text_high) *(.text_high2) }
}

//--- asm

 .syntax unified
 .section .text_low, "ax", %progbits
 .thumb
 .globl _start
_start: bx lr
 .globl low_target
 .type low_target, %function
low_target:
 bl high_target
 bl high_target2

 .section .text_low2, "ax", %progbits
 .thumb
 .globl low_target2
 .type low_target2, %function
low_target2:
 bl high_target
 bl high_target2

 .section .text_high, "ax", %progbits
 .thumb
 .globl high_target
 .type high_target, %function
high_target:
 bl low_target
 bl low_target2

 .section .text_high2, "ax", %progbits
 .thumb
 .globl high_target2
 .type high_target2, %function
high_target2:
 bl low_target
 bl low_target2

// STATIC: Contents of section .text_low:
// STATIC-NEXT: 0094 704700f0 03f800f0 09f80000 7847c046
// STATIC-NEXT: 00a4 00c09fe5 1cff2fe1 01000002 7847c046
// STATIC-NEXT: 00b4 00c09fe5 1cff2fe1 29000002 fff7eeff
// STATIC-NEXT: 00c4 fff7f4ff
// STATIC: Contents of section .text_high:
// STATIC-NEXT: 2000000 00f002f8 00f008f8 7847c046 00c09fe5
// STATIC-NEXT: 2000010 1cff2fe1 97000000 7847c046 00c09fe5
// STATIC-NEXT: 2000020 1cff2fe1 c1000000 fff7eeff fff7f4ff

// PICVENEER: Contents of section .text_low:
// PICVENEER-NEXT: 0094 704700f0 03f800f0 0bf80000 7847c046
// PICVENEER-NEXT: 00a4 04c09fe5 0cc08fe0 1cff2fe1 51ffff01
// PICVENEER-NEXT: 00b4 7847c046 04c09fe5 0cc08fe0 1cff2fe1
// PICVENEER-NEXT: 00c4 6dffff01 fff7eaff fff7f2ff
// PICVENEER: Contents of section .text_high:
// PICVENEER-NEXT: 2000000 00f002f8 00f00af8 7847c046 04c09fe5
// PICVENEER-NEXT: 2000010 0cc08fe0 1cff2fe1 7f0000fe 7847c046
// PICVENEER-NEXT: 2000020 04c09fe5 0cc08fe0 1cff2fe1 9d0000fe
// PICVENEER-NEXT: 2000030 fff7eaff fff7f2ff
