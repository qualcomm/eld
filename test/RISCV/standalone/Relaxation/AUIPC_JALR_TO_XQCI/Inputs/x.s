  .text
  .p2align 1
  .type f, @function
f:
  ret
  .size f, .-f

  .p2align 1
  .globl main
  .type main, @function
main:
  call f
  tail f

  .org 0x780
  call f
  tail f

  .org 0x880
  call f
  tail f

  .org 0xfff00
  call f
  tail f

  .org 0x100100
  call f
  tail f

  .size main, .-main
