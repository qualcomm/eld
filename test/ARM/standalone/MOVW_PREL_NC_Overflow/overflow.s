//---overflow.s--------- Executable-----------------------------------#

// BEGIN_COMMENT
// Linker should not check for relocation overflow for R_ARM_MOVW_PREL_NC
// END_COMMENT
// START_TEST
// RUN: %llvm-mc %s -filetype=obj -triple=armv8a-unknown-linux-gnueabi -o %t.o
// RUN: %link %t.o 2>&1 | %filecheck %s --allow-empty
// RUN: %readelf -r %t.o | %filecheck %s --check-prefix=RELOC
// RELOC: Relocation section '.rel.R_ARM_MOVW_PREL_NC' at offset 0x20084 contains 5 entries:
// RELOC-NEXT:  Offset     Info    Type                Sym. Value  Symbol's Name
// RELOC-NEXT: 00000000  0000022d R_ARM_MOVW_PREL_NC     00000000   label
// RELOC-NEXT: 00000004  0000032d R_ARM_MOVW_PREL_NC     00000004   label1
// RELOC-NEXT: 00000008  0000042d R_ARM_MOVW_PREL_NC     00000008   label2
// RELOC-NEXT: 0000000c  0000052d R_ARM_MOVW_PREL_NC     0000fffc   label3
// RELOC-NEXT: 00000010  0000052d R_ARM_MOVW_PREL_NC     0000fffc   label3
// CHECK-NOT: Relocation overflow when applying relocation `R_ARM_MOVW_PREL_NC
 .syntax unified
 .globl _start
 .align 12
_start:
.section .R_ARM_MOVW_PREL_NC, "ax",%progbits
.align 8
 movw r0, :lower16:label - .
 movw r1, :lower16:label1 - .
 movw r2, :lower16:label2 + 4 - .
 movw r3, :lower16:label3 - .
 movw r4, :lower16:label3 + 0x2214 - .

.section .destination, "aw",%progbits
 .balign 65536
/// 0x20000
label:
 .word 0
/// 0x20004
label1:
 .word 1
/// 0x20008
label2:
 .word 2
/// Test label3 is immediately below 2^16 alignment boundary
 .space 65536 - 16
/// 0x2fffc
label3:
 .word 3
/// label3 + 4 is on a 2^16 alignment boundary
label3.4:
 .word 4
// END_TEST