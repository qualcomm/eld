// Assembly file with unsupported .sframe version
  .text
  .globl _start
  .type _start, %function
_start:
  .space 4
  .size _start, .-_start

  .section .sframe,"a",%0x6ffffff4
  // Invalid SFrame header (bad version)
  .short 0xdee2          // magic
  .byte  99              // bad version (should be 2)
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
