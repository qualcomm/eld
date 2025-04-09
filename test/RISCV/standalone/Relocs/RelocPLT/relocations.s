# RUN: %clang %clangopts -O3 -fPIC -c %p/Inputs/f.c -o %t.f.o
# RUN: %link %linkopts -shared %t.f.o -o %t.f.so

# RUN: %clang %clangopts -fno-pic -mno-relax -c %s -o %t.o
# RUN: %link %linkopts -o %t.out --no-align-segments \
# RUN:  --section-start .data=0x1000 \
# RUN:  --section-start .text=0xa00000 \
# RUN:  --section-start .plt=0xabc000 %t.o %t.f.so
# RUN: %readelf -x .data -d %t.out | %filecheck %s --check-prefix=DATA
# RUN: %objdump -d %t.out | %filecheck %s --check-prefix=INSTR

.data
.long f
.quad f

# DATA: Hex dump of section '.data':
# DATA-NEXT: 20c0ab00 20c0ab00 00000000

.text
lui t1, %hi(f)
# INSTR:      a00000: 00abc337      lui t1, 0xabc

lui t1, %hi(f+4)
# INSTR-NEXT: a00004: 00abc337      lui t1, 0xabc

addi t1, t1, %lo(f)
# INSTR-NEXT: a00008: 02030313      addi t1, t1, 0x20

addi t1, t1, %lo(f+4)
# INSTR-NEXT: a0000c: 02430313      addi t1, t1, 0x24

sb t1, %lo(f)(a2)
# INSTR-NEXT: a00010: 02660023      sb t1, 0x20(a2)

sb t1, %lo(f+4)(a2)
# INSTR-NEXT: a00014: 02660223      sb t1, 0x24(a2)

auipc t1, %pcrel_hi(f)
# INSTR-NEXT: a00018: 000bc317     	auipc	t1, 0xbc

auipc t1, %pcrel_hi(f+4)
# INSTR-NEXT: a0001c: 000bc317     	auipc	t1, 0xbc

j f
# INSTR-NEXT: a00020: 000bc06f      j 0xabc020

call f
# INSTR-NEXT: a00024: 000bc097     	auipc	ra, 0xbc
# INSTR-NEXT: a00028: ffc080e7     	jalr	-0x4(ra)
