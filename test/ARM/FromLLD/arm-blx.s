// RUN: %clang %clangopts -target armv7a-none-linux-gnueabi -c %s -o %t
// RUN: %clang %clangopts -target armv7a-none-linux-gnueabi -c %S/Inputs/far-arm-thumb-abs.s -o %tfar
// RUN: echo "SECTIONS { \
// RUN:          . = 0xb4; \
// RUN:          .callee1 : { *(.callee_low) } \
// RUN:          .callee2 : { *(.callee_arm_low) } \
// RUN:          .caller : { *(.text) } \
// RUN:          .callee3 : { *(.callee_high) } \
// RUN:          .callee4 : { *(.callee_arm_high) } } " > %t.script
// RUN: %link --script %t.script %t %tfar -o %t2
// RUN: %objdump -d --triple=armv7a-none-linux-gnueabi %t2 |%filecheck %s

// Test BLX instruction is chosen for ARM BL/BLX instruction and Thumb callee
// Using two callees to ensure at least one has 2-byte alignment.
 .syntax unified
 .thumb
 .section .callee_low, "ax",%progbits
 .align 2
 .type callee_low,%function
callee_low:
 bx lr
 .type callee_low2, %function
callee_low2:
 bx lr

 .section .callee_arm_low, "ax",%progbits
 .arm
 .balign 0x100
 .type callee_arm_low,%function
 .align 2
callee_arm_low:
  bx lr

.section .text, "ax",%progbits
 .arm
 .globl _start
 .balign 0x10000
 .type _start,%function
_start:
 bl  callee_low
 blx callee_low
 bl  callee_low2
 blx callee_low2
 bl  callee_high
 blx callee_high
 bl  callee_high2
 blx callee_high2
 bl  blx_far
 blx blx_far2
// blx to ARM instruction should be written as a BL
 bl  callee_arm_low
 blx callee_arm_low
 bl  callee_arm_high
 blx callee_arm_high
 bx lr

 .section .callee_high, "ax",%progbits
 .balign 0x100
 .thumb
 .type callee_high,%function
callee_high:
 bx lr
 .type callee_high2,%function
callee_high2:
 bx lr

 .section .callee_arm_high, "ax",%progbits
 .arm
 .balign 0x100
 .type callee_arm_high,%function
callee_arm_high:
  bx lr

#CHECK: Disassembly of section .callee1:
#CHECK: 000000b4 <callee_low>:
#CHECK:       b4: 4770          bx      lr
#CHECK: 000000b6 <callee_low2>:
#CHECK:       b6: 4770          bx      lr
#CHECK: Disassembly of section .callee2:
#CHECK: 00000100 <callee_arm_low>:
#CHECK:      100: e12fff1e      bx      lr
#CHECK: Disassembly of section .caller:
#CHECK: 00010000 <_start>:
#CHECK:    10000: faffc02b      blx     0xb4 <callee_low>       @ imm = #-0xff54
#CHECK:    10004: faffc02a      blx     0xb4 <callee_low>       @ imm = #-0xff58
#CHECK:    10008: fbffc029      blx     0xb6 <callee_low2>      @ imm = #-0xff5a
#CHECK:    1000c: fbffc028      blx     0xb6 <callee_low2>      @ imm = #-0xff5e
#CHECK:    10010: fa00003a      blx     0x10100 <callee_high>   @ imm = #0xe8
#CHECK:    10014: fa000039      blx     0x10100 <callee_high>   @ imm = #0xe4
#CHECK:    10018: fb000038      blx     0x10102 <callee_high2>  @ imm = #0xe2
#CHECK:    1001c: fb000037      blx     0x10102 <callee_high2>  @ imm = #0xde
#CHECK:    10020: eb000005      bl      0x1003c <__blx_far_A2T_veneer@island-1> @ imm = #0x14
#CHECK:    10024: eb000007      bl      0x10048 <__blx_far2_A2T_veneer@island-2> @ imm = #0x1c
#CHECK:    10028: ebffc034      bl      0x100 <callee_arm_low>  @ imm = #-0xff30
#CHECK:    1002c: ebffc033      bl      0x100 <callee_arm_low>  @ imm = #-0xff34
#CHECK:    10030: eb000072      bl      0x10200 <callee_arm_high> @ imm = #0x1c8
#CHECK:    10034: eb000071      bl      0x10200 <callee_arm_high> @ imm = #0x1c4
#CHECK:    10038: e12fff1e      bx      lr
#CHECK: 0001003c <__blx_far_A2T_veneer@island-1>:
#CHECK:    1003c: e59fc000      ldr     r12, [pc]               @ 0x10044 <__blx_far_A2T_veneer@island-1+0x8>
#CHECK:    10040: e12fff1c      bx      r12
#CHECK:    10044:       25 00 01 02     .word   0x02010025
#CHECK: 00010048 <__blx_far2_A2T_veneer@island-2>:
#CHECK:    10048: e59fc000      ldr     r12, [pc]               @ 0x10050 <__blx_far2_A2T_veneer@island-2+0x8>
#CHECK:    1004c: e12fff1c      bx      r12
#CHECK:    10050:       29 00 01 02     .word   0x02010029
#CHECK: Disassembly of section .callee3:
#CHECK: 00010100 <callee_high>:
#CHECK:    10100: 4770          bx      lr
#CHECK: 00010102 <callee_high2>:
#CHECK:    10102: 4770          bx      lr
#CHECK: Disassembly of section .callee4:
#CHECK: 00010200 <callee_arm_high>:
#CHECK:    10200: e12fff1e      bx      lr
