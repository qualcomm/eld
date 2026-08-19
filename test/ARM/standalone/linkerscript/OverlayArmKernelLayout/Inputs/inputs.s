.syntax unified
.arm

.section .vectors, "ax", %progbits
  .space 0x20

.section .vectors.bhb.loop8, "ax", %progbits
  .space 0x40

.section .vectors.bhb.bpiall, "ax", %progbits
  .space 0x60

.section .stubs, "ax", %progbits
  .space 0x350

.section .init.text, "ax", %progbits
.global _start
_start:
  .space 0x1000
