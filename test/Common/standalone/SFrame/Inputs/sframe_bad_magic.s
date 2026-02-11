// Assembly file with invalid .sframe magic number
  .text
  .globl _start
  .type _start, %function
_start:
  .space 4
  .size _start, .-_start

  .section .sframe,"a",%0x6ffffff4
  // Invalid SFrame header (bad magic)
  .short 0xbeef          // bad magic (should be 0xdee2)
  .byte  2               // version
  .byte  0               // flags
  .byte  3               // abi
  .byte  0               // cfa_fixed_fp_offset
  .byte  0               // cfa_fixed_ra_offset
  .byte  0               // auxhdr_len
  .long  0               // num_fdes
  .long  0               // num_fres
  .long  0               // reserved
  .long  0               // reserved
  .long  0               // fre_len
  .long  0               // fde_len
