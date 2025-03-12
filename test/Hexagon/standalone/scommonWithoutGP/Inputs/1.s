  .text
  .file "ld-temp.o"
  .section  .text.main,"ax",@progbits
  .globl  main
  .falign
  .type main,@function
main:
  {
    allocframe(#8)
  }
  {
    r2 = #0
  }
  {
    memw(r29+#4) = r2
  }
  {
    r0 = memub(##a)
  }
  {
    dealloc_return
  }
.Ltmp0:
  .size main, .Ltmp0-main

  .type a,@object
  .section  .scommon.1,"aw",@progbits
a:
  .byte 0
  .size a, 1

