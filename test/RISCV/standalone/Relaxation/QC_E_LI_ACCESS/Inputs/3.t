/* GP placed so that GP-relative and absolute ranges don't overlap:
 * GP = 0x10000000 (268435456)
 *
 * GP-std range:    isInt<12>(sym - GP) → sym in [GP-2048 .. GP+2047]
 * Abs-std range:   isInt<12>(sym)      → sym in [1 .. 2047]  (non-zero)
 * GP-xqci range:   isInt<26>(sym - GP) → sym in [GP-33554432 .. GP+33554431]
 * Abs-xqci range:  isInt<26>(sym)      → sym in [1 .. 33554431] (non-zero)
 *
 * With GP = 0x10000000:
 *   Abs range [1..33554431] = [0x1..0x1FFFFFF] lies entirely below GP,
 *   so GP-relative and absolute ranges do not overlap.
 */
__global_pointer$ = 0x10000000;

/* GP-std: boundary symbols at GP ± 2047 (innermost) and GP ± 2048 (just out) */
sym_gp_std_pos_edge   = __global_pointer$ + 2047;   /* isInt<12>(+2047) → GP-std  */
sym_gp_std_neg_edge   = __global_pointer$ - 2048;   /* isInt<12>(-2048) → GP-std  */
sym_gp_std_pos_out    = __global_pointer$ + 2048;   /* isInt<12>(+2048) → false → GP-xqci */
sym_gp_std_neg_out    = __global_pointer$ - 2049;   /* isInt<12>(-2049) → false → GP-xqci */

/* GP-xqci: boundary symbols at GP ± 33554431 (innermost) and GP ± 33554432 (just out) */
sym_gp_xqci_pos_edge  = __global_pointer$ + 33554431; /* isInt<26>(+33554431) → GP-xqci */
sym_gp_xqci_neg_edge  = __global_pointer$ - 33554432; /* isInt<26>(-33554432) → GP-xqci */
sym_gp_xqci_pos_out   = __global_pointer$ + 33554432; /* isInt<26>(+33554432) → false → miss */
sym_gp_xqci_neg_out   = __global_pointer$ - 33554433; /* isInt<26>(-33554433) → false → miss */

/* Abs-std: boundary symbols at ± 2047 (innermost) and ± 2048 (just out) */
sym_abs_std_edge      = 2047;   /* isInt<12>(2047) → Abs-std (if ZeroRelax) */
sym_abs_std_out       = 2048;   /* isInt<12>(2048) → false → Abs-xqci */

/* Abs-xqci: boundary symbols at 33554431 (innermost) and 33554432 (just out) */
sym_abs_xqci_edge     = 33554431; /* isInt<26>(33554431) → Abs-xqci */
sym_abs_xqci_out      = 33554432; /* isInt<26>(33554432) → false → miss */

SECTIONS { .text : { *(.text) } }
