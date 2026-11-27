        .section .text.live,"ax",@progbits
        .globl _start
_start:
        # Nonalloc-only undefined refs are ignored by default; root this one.
        .reloc ., R_RISCV_32, missing
        .word 0

#ifdef EXPLICIT_DISCARD
        .section .discarded,"ax",@progbits
#else
        .section .text.dead,"ax",@progbits
#endif
        .globl dead_end
dead_end:
        .word 0

        .section .debug_pairs32,"",@progbits
        .reloc ., R_RISCV_ADD32, dead_end+5
        .reloc ., R_RISCV_SUB32, missing
        .word 7

        .section .debug_pairs_uleb,"",@progbits
        .reloc ., R_RISCV_SET_ULEB128, dead_end+5
        .reloc ., R_RISCV_SUB_ULEB128, missing
        .byte 0x80, 0
