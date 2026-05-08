/* GP placed far enough from zero that the GP-relative and absolute symbol
 * ranges do not overlap. */
__global_pointer$ = 0x10000000;

/* Within isInt<12> of GP */
sym_gprel_std   = __global_pointer$ + 0x20;

/* Within isInt<26> of GP, but not isInt<12> */
sym_gprel_xqci  = __global_pointer$ + 0x100000;

/* Within isInt<12> of 0 */
sym_abs_std     = 0x100;

/* Within isInt<26> of 0, but not isInt<12> */
sym_abs_xqci    = 0x200000;

/* Outwith isInt<26> of both 0 and GP. */
sym_too_far     = __global_pointer$ + 0x4000000;

SECTIONS {
  .text : { *(.text) }
}
