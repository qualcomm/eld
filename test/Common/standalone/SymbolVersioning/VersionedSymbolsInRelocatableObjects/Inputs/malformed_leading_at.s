// Global symbol named literally `@V1` (no base before the `@`).
// eld must reject this as a malformed versioned symbol name.
.data
.globl "@V1"
"@V1":
    .byte 0
