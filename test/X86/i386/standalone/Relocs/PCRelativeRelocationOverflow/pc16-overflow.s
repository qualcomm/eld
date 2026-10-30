## R_386_PC16 accepts values through 65535 and rejects 65536. ELF32's
## relocation arithmetic wraps at 32 bits before this narrow-field check.

# RUN: %llvm-mc -filetype=obj %p/Inputs/pcrel-pc16.s -o %t.o
# RUN: %link %linkopts -m elf_i386 -T %p/Inputs/pcrel-pc16-fit.t %t.o -o %t.fit
# RUN: not %link %linkopts -m elf_i386 -T %p/Inputs/pcrel-pc16-overflow.t %t.o -o %t.overflow 2>&1 | %filecheck %s

# CHECK: relocation R_386_PC16 out of range: 65536 is not in [-65536, 65535]
