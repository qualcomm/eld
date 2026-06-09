.global _start
_start:
  bl main
  mov r7, #1
  swi 0