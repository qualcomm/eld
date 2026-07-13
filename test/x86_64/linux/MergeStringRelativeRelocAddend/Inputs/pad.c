/* pad.c -- compiled and linked FIRST.
 *
 * Provides the canonical (kept) copies of the short strings "a", "c" and "fF",
 * plus the escape strings, so that main.c's duplicate copies of "a", "c" and
 * "fF" are excluded during string merging.
 *
 * The unique padding string below ensures pad.o's fragment isn't empty and
 * gives the escape strings a nonzero output offset in the merged .rodata --
 * enough to create the mismatch between main.o's small pre-merge input offset
 * for "  \n" (7) and its true post-merge address, which the doPostLayout
 * fixup must correct. */
const char *pad01 = "pad_string_alpha";

/* The escape strings are declared before "a"/"c"/"fF" (matching the layout
 * that triggers the bug): this places the canonical short strings at the end
 * of pad.o's string table, so that main.o's excluded copies of them shift the
 * escape strings to a different output offset than their input offset. */
const char *p_nl = "  \\n";
const char *p_cr = "  \\r";
const char *p_tab = "  \\t";
const char *p_nul = "  \\0";

const char *p_a = "a";
const char *p_c = "c";
const char *p_fF = "fF";
