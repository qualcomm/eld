.section .text.main,"ax",@progbits
.globl main
main:
  auipc a0, %pcrel_hi(__var1)
  addi  a0, a0, %pcrel_lo(main)
  auipc a0, %pcrel_hi(__var2)
  addi  a0, a0, %pcrel_lo(main+8)
  auipc a0, %pcrel_hi(__var3)
  addi  a0, a0, %pcrel_lo(main+16)
  auipc a0, %pcrel_hi(__var4)
  addi  a0, a0, %pcrel_lo(main+24)
.size main, .-main
