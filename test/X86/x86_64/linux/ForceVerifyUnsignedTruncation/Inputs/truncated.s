# Values one past the maximum of the relocation field.
.globl _start
_start:
.reloc ., R_X86_64_8, big8
.byte 0
.reloc ., R_X86_64_16, big16
.short 0
.set big8, 0x100
.set big16, 0x10000
