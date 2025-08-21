.section .tdata, "awT", @progbits
.align 4

.global u
u:
  .long 0x0c        

.global v
v:
  .long 0x2a      
.section .data
.align 8

offset_to_u:
  .quad 0x0        
.reloc offset_to_u, R_X86_64_TPOFF64, u

offset_to_v:
  .quad 0x0         
.reloc offset_to_v, R_X86_64_TPOFF64, v
