.text
.weak external
external:
  auipc sp,%pcrel_hi(external)
  addi sp,sp,%pcrel_lo(external)+0xf000
  sw sp, %pcrel_lo(external)+0xf000(sp)
tlsdesc:
  auipc a0, %tlsdesc_hi(tbss_var)
  lw a1, %tlsdesc_load_lo(tlsdesc)+0xf000(a0)
  addi a0, a0, %tlsdesc_add_lo(tlsdesc)+0xf000
  jalr t0, 0(a1), %tlsdesc_call(tlsdesc)+0xf000

.section .tbss
.globl tbss_var
tbss_var:
.zero 4
