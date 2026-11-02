/* Defsyms:
     FEAT=<n>  feature bits written to the x86 property
     NONOTE    emit no .note.gnu.property
     BAD       make n_descsz larger than the section
 */
.ifndef NONOTE
  .section ".note.gnu.property", "a"
  .p2align 2
  .long 4             /* n_namesz */
.ifdef BAD
  .long 64            /* n_descsz: too large */
.else
  .long 12            /* n_descsz: property header and feature value */
.endif
  .long 5             /* NT_GNU_PROPERTY_TYPE_0 */
  .asciz "GNU"
  .long 0xc0000002    /* GNU_PROPERTY_X86_FEATURE_1_AND */
  .long 4             /* pr_datasz */
  .long FEAT
.endif

  .data
d:
  .long 0
