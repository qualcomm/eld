/* Defsyms:
     FEAT=<n>  feature bits written to the RISC-V property
     NONOTE    emit no .note.gnu.property
     BAD       omit the ELF64 property padding
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

.ifndef ELF32
.set ELF32, 0
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
.if BAD && ELF32
.long 8
.else
.long 4
.endif
.long FEAT

.if !BAD
.if !ELF32
.p2align 3
.endif
.endif

.Ldesc_end:
.endif

.section .data, "aw", @progbits
.long 1
