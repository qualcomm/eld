.section ".note.gnu.property", "a"
.p2align 3
.long 4
.long 0x10
.long 0x5
.asciz "QCT"

.long 0xc0000000 // GNU_PROPERTY_AARCH64_FEATURE_1_AND
.long 4
.long 1          // GNU_PROPERTY_AARCH64_FEATURE_1_BTI
.long 0

.long 4
.long 0x10
.long 0x5
.asciz "QCT"
.long 0xc0000000 // GNU_PROPERTY_AARCH64_FEATURE_1_AND
.long 4
.long 2          // GNU_PROPERTY_AARCH64_FEATURE_1_PAC
.long 0

