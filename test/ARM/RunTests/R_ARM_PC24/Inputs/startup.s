.syntax unified
.arm

.global _start
.type _start, %function
_start:
    @ Force R_ARM_PC24 relocation explicitly on a BL instruction.
    @ 0xebfffffe = ARM BL with offset field=-2 the standard assembler encoding
    @ for an unresolved branch; the -8 pipeline bias is baked in as addend.
    .reloc _start, R_ARM_PC24, main
    .inst 0xebfffffe
    mov r7, #1
    swi 0

