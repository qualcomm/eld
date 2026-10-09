/* Defsyms:
     FEAT=<n>  feature bits written to both properties
     NONOTE    emit no .note.gnu.property
     BAD       omit the final ELF64 property padding
 */
.ifndef FEAT
.set FEAT, 0
.endif

.ifndef BAD
.set BAD, 0
.endif

.ifndef NONOTE
.set NONOTE, 0
.endif

.if !NONOTE
.section .note.gnu.property, "a", @note
.p2align 2

.long 4
.long .Ldesc_end - .Ldesc_begin
.long 5
.asciz "GNU"

.Ldesc_begin:
.long 0xc0000000
.long 4
.long FEAT
.p2align 3
.long 0xc0000002
.long 4
.long FEAT
.if !BAD
.p2align 3
.endif

.Ldesc_end:
.endif

.section .data, "aw", @progbits
.long 1
