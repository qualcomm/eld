/* GP at a standard address */
__global_pointer$ = 0x01000000;

/* Abs-std: 0x100 is within isInt<12> (signed 12-bit range = -2048..2047) */
sym_abs_std     = 0x100;

/* GP-std range: small positive offset from GP, within isInt<12>(sym - GP) */
sym_gprel_std   = __global_pointer$ + 0x20;

SECTIONS { .text : { *(.text) } }
