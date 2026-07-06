## A partial link should keep a pcrel_lo relocation pointing at the label on its
## paired pcrel_hi instruction, not at the final symbol. The partial output is
## then linked to a final image to confirm the preserved pair still resolves.

# RUN: %llvm-mc -filetype=obj -triple=riscv32 -mattr=-relax %s -o %t.o
# RUN: %link %linkopts -o %t.part.o %t.o -r
# RUN: %readelf -r %t.part.o | %filecheck %s
# RUN: %link -m elf32lriscv --defsym=gvar=0x8000 -Ttext=0x1000 -e nonpaired %t.part.o -o %t.elf
# RUN: %objdump -d %t.elf | %filecheck --check-prefix=DISASM %s

# CHECK:      R_RISCV_PCREL_LO12_I {{[0-9a-f]+}} .Lutype
# CHECK-NEXT: R_RISCV_PCREL_HI20 {{[0-9a-f]+}} gvar
# CHECK-NEXT: R_RISCV_PCREL_LO12_S {{[0-9a-f]+}} .Lutype

# DISASM:      lw a0, -0x10(a1)
# DISASM:      auipc a1, 0x7
# DISASM:      sw t1, -0x10(a1)

	.text
	.globl	nonpaired
nonpaired:
	j	.Lutype                      ## break HI20/LO12 adjacency
.Lload:
	lw	a0, %pcrel_lo(.Lutype)(a1)   ## PCREL_LO12_I, sym = .Lutype (HI20 site)
	addi	t1, a0, 42
	j	.Lstore
.Lutype:
	auipc	a1, %pcrel_hi(gvar)          ## PCREL_HI20,   sym = gvar (final target)
	j	.Lload
.Lstore:
	sw	t1, %pcrel_lo(.Lutype)(a1)   ## PCREL_LO12_S, sym = .Lutype (HI20 site)
	ret
