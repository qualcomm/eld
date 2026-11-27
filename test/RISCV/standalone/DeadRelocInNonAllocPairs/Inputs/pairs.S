        .section .text.live,"ax",@progbits
        # Local controls stay live; the global variant exercises shouldIgnore.
        .globl _start
#ifdef GLOBAL_LIVE_END
        .globl live_end
#endif
_start:
        .space 16
live_end:
        .byte 0

#ifdef EXPLICIT_DISCARD
        .section .discarded,"ax",@progbits
#else
        .section .text.dead,"ax",@progbits
#endif
        .globl dead, dead_end
dead:
        .space 16
dead_end:
        .byte 0

        .section .debug_pairs32,"",@progbits
        # dead/dead, dead/live, live/dead, live/live; addends are deliberate.
        .reloc ., R_RISCV_ADD32, dead_end+5
        .reloc ., R_RISCV_SUB32, dead+3
        .word 7
        .reloc ., R_RISCV_ADD32, dead_end+5
        .reloc ., R_RISCV_SUB32, _start-32
        .word 7
        .reloc ., R_RISCV_ADD32, live_end+5
        .reloc ., R_RISCV_SUB32, dead+3
        .word 7
        .reloc ., R_RISCV_ADD32, live_end+5
        .reloc ., R_RISCV_SUB32, _start+3
        .word 7

        # A dead middle operand must survive both following live operations.
        .reloc ., R_RISCV_ADD32, live_end+5
        .reloc ., R_RISCV_SUB32, dead+3
        .reloc ., R_RISCV_ADD32, live_end+5
        .reloc ., R_RISCV_SUB32, _start+3
        .word 7
        .reloc ., R_RISCV_ADD32, live_end+5
        .reloc ., R_RISCV_SUB32, _start+3
        .reloc ., R_RISCV_ADD32, live_end+5
        .reloc ., R_RISCV_SUB32, _start+3
        .word 7

        .section .debug_pairs_uleb,"",@progbits
        # The same combinations, with two bytes of ULEB128 storage per pair.
        .reloc ., R_RISCV_SET_ULEB128, dead_end+5
        .reloc ., R_RISCV_SUB_ULEB128, dead+3
        .byte 0x80, 0
        .reloc ., R_RISCV_SET_ULEB128, dead_end+5
        .reloc ., R_RISCV_SUB_ULEB128, _start-32
        .byte 0x80, 0
        .reloc ., R_RISCV_SET_ULEB128, live_end+5
        .reloc ., R_RISCV_SUB_ULEB128, dead+3
        .byte 0x80, 0
        .reloc ., R_RISCV_SET_ULEB128, live_end+5
        .reloc ., R_RISCV_SUB_ULEB128, _start-32
        .byte 0x80, 0
