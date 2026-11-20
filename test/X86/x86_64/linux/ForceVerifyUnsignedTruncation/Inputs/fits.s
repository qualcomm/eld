# Values that fit exactly in the relocation field, including the maximum.
.globl _start
_start:
.reloc ., R_X86_64_8, small8
.byte 0
.reloc ., R_X86_64_8, max8
.byte 0
.reloc ., R_X86_64_16, small16
.short 0
.reloc ., R_X86_64_16, max16
.short 0
.set small8, 5
.set max8, 0xff
.set small16, 5
.set max16, 0xffff
