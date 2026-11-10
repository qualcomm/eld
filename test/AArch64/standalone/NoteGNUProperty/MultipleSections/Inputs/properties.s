// Assembly is needed to create two distinct GNU property sections in one object.
.section .note.gnu.property,"a",@note,unique,1
.p2align 3
.long 4                     // Name size
.long 16                    // Descriptor size
.long 5                     // NT_GNU_PROPERTY_TYPE_0
.asciz "GNU"
.long 0xc0000000            // GNU_PROPERTY_AARCH64_FEATURE_1_AND
.long 4                     // Feature data size
.long FIRST
.long 0                     // Padding

.section .note.gnu.property,"a",@note,unique,2
.p2align 3
.long 4
.long 16
.long 5
.asciz "GNU"
.long 0xc0000000
.long 4
.long SECOND
.long 0
