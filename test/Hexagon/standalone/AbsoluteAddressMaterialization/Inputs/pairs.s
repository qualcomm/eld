# Explicit words/relocations exercise malformed inputs and relocation ordering
# independently of the assembler's normal packet construction.
.text
.globl entry
.hidden data, high
entry:
.if CASE == 0
  .word 0x6a49c000
  .reloc 0, R_HEX_6_PCREL_X, data
.elseif CASE == 1
  .word 0x00004000, 0x6a49c000
  .reloc 0, R_HEX_B32_PCREL_X, data
.elseif CASE == 2
  .word 0x00004000, 0x6a49c000
  .reloc 0, R_HEX_B32_PCREL_X, high
  .reloc 4, R_HEX_6_PCREL_X, data+4
.elseif CASE == 3
  .word 0x00004000, 0x6a49c000
  .reloc 0, R_HEX_B32_PCREL_X, data
  .reloc 4, R_HEX_6_PCREL_X, data+5
.elseif CASE == 4
  .word 0x00004000, 0x6a49c000
  .reloc 0, R_HEX_B32_PCREL_X, data
  .reloc 4, R_HEX_6_PCREL_X, data+4
  .reloc 4, R_HEX_6_PCREL_X, data+4
.elseif CASE == 5
  .word 0x00004000, 0x7800c000
  .reloc 0, R_HEX_B32_PCREL_X, data
  .reloc 4, R_HEX_6_PCREL_X, data+4
.elseif CASE == 6
  .word 0x0000c000, 0x6a49c000
  .reloc 0, R_HEX_B32_PCREL_X, data
  .reloc 4, R_HEX_6_PCREL_X, data+4
.elseif CASE == 7
  .word 0x00004000, 0x6a494000
  .reloc 0, R_HEX_B32_PCREL_X, data
  .reloc 4, R_HEX_6_PCREL_X, data+4
.elseif CASE == 8
  .word 0x7f004000, 0x7f004000, 0x7f004000, 0x00004000, 0x6a49c000
  .reloc 12, R_HEX_B32_PCREL_X, data+12
  .reloc 16, R_HEX_6_PCREL_X, data+16
.elseif CASE == 9
  # Valid pair whose relocation entries are deliberately reversed.
  .word 0x00004000, 0x6a49c000
  .reloc 4, R_HEX_6_PCREL_X, data+4
  .reloc 0, R_HEX_B32_PCREL_X, data
.elseif CASE == 10
  # Extender is not at packet offset zero. Preserve register r17 and the end
  # marker, and normalize BOTH addends, not just the consumer's addend.
  .word 0x7f004000, 0x00004000, 0x6a49c011
  .reloc 4, R_HEX_B32_PCREL_X, data+4
  .reloc 8, R_HEX_6_PCREL_X, data+8
.elseif CASE == 11
  .word 0x00004000, 0x6a49c000
  .reloc 0, R_HEX_B32_PCREL_X, data
  .reloc 4, R_HEX_6_PCREL_X, data+4
  .reloc 4, R_HEX_NONE, data
.elseif CASE == 12
  .word 0x78004000, 0x6a49c000
  .reloc 0, R_HEX_B32_PCREL_X, data
  .reloc 4, R_HEX_6_PCREL_X, data+4
.elseif CASE == 13
  { jumpr r31 }
  .section .text.dead,"ax",@progbits
  .word 0x6a49c000
  .reloc 0, R_HEX_6_PCREL_X, data
.endif
