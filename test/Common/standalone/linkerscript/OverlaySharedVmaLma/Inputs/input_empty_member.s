.section .small, "ax", %progbits
  .space 0x20

.section .empty, "ax", %progbits

.section .large, "ax", %progbits
  .space 0x100

.section .after, "ax", %progbits
.global _start
_start:
  .space 0x10
