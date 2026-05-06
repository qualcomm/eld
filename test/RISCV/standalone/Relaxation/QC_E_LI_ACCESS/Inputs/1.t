
/* GP somewhere not reachable by a standard 12-bit GP-relative access */
__global_pointer$ = 0x01000000;

/* In 12-bit GP-relative range: GP + 0x20 */
sym_gprel_std   = __global_pointer$ + 0x20;

/* In 26-bit GP-relative range but not 12-bit: GP + 0x100000 */
sym_gprel_xqci  = __global_pointer$ + 0x100000;

/* Out of all ranges: GP + 0x4000000 > 2^25 */
sym_too_far     = __global_pointer$ + 0x4000000;

SECTIONS {
  .text : { *(.text) }
}
