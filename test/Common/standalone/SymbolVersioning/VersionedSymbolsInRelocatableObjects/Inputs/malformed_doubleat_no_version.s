// Global symbol named literally `foo@@` (no version after `@@`).
// eld must reject this as a malformed versioned symbol name.
.data
.globl "foo@@"
"foo@@":
    .byte 0
