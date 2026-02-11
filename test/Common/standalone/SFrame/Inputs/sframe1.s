// Assembly file with a valid .sframe section (SFrame v2)
  .text
  .globl _start
  .type _start, %function
_start:
  .space 4
  .size _start, .-_start

  .section .sframe,"a",%0x6ffffff4
  // SFrame header (28 bytes)
  .short 0xdee2          // magic
  .byte  2               // version
  .byte  0               // flags
  .byte  3               // abi (AMD64)
  .byte  0               // cfa_fixed_fp_offset
  .byte  0               // cfa_fixed_ra_offset
  .byte  0               // auxhdr_len
  .long  0               // num_fdes
  .long  0               // num_fres
  .long  0               // reserved
  .long  0               // reserved
  .long  0               // fre_len
  .long  0               // fde_len
