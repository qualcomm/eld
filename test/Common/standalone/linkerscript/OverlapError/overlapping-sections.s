#--overlapping-sections.s--------------------- Executable,LS------------------#

#BEGIN_COMMENT
# This tests checks that the linker is able to detect overlaps (VMA/LMA)
#END_COMMENT

# RUN: %clang %clangopts -c %s -o %t.o

# RUN: echo "SECTIONS { \
# RUN:   .sec1 0x8000 : AT(0x8000) { sec1_start = .; *(.first_sec) sec1_end = .;} \
# RUN:   .sec2 0x8800 : AT(0x8080) { sec2_start = .; *(.second_sec) sec2_end = .;} \
# RUN: }" > %t-lma.script
# RUN: %not %link %linkopts -o /dev/null -T %t-lma.script %t.o -z max-page-size=0x1000 -shared  2>&1 | FileCheck %s -check-prefix LMA-OVERLAP-ERR
# LMA-OVERLAP-ERR:      Error: section .sec1 load address range overlaps with .sec2
# LMA-OVERLAP-ERR-NEXT: >>> .sec1 range is [0x8000, 0x80FF]
# LMA-OVERLAP-ERR-NEXT: >>> .sec2 range is [0x8080, 0x817F]

# Check that we create the expected binary with --no-check-sections:
# RUN: %link %linkopts -z max-page-size=0x1000 -o %t.so --script %t-lma.script %t.o -shared --no-check-sections

# Verify that the .sec2 was indeed placed in a PT_LOAD where the PhysAddr
# overlaps with where .sec1 is loaded:
# RUN: llvm-readelf --sections -l %t.so | FileCheck %s -check-prefix BAD-LMA
# BAD-LMA-LABEL: Section Headers:
# BAD-LMA:  .sec1             PROGBITS        {{[0]+}}8000 {{.*}} 000100 00  WA  0   0  1
# BAD-LMA-NEXT:   .sec2             PROGBITS        {{[0]+}}8800 {{.*}} 000100 00  WA  0   0  1
# BAD-LMA-LABEL: Program Headers:
# BAD-LMA:  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
# BAD-LMA:  LOAD           0x{{[0]+}}2000 0x{{[0]+}}8000 0x{{[0]+}}8000 0x{{[0]+}}100 0x{{[0]+}}100 RW  0x1000
# BAD-LMA-NEXT:  LOAD           0x{{[0]+}}2800 0x{{[0]+}}8800 0x{{[0]+}}8080 0x{{[0]+}}100 0x{{[0]+}}100 RW  0x1000

# Now try a script where the virtual memory addresses overlap but ensure that the
# load addresses don't:
# RUN: echo "SECTIONS { \
# RUN:   .sec1 0x8000 : AT(0x8000) { sec1_start = .; *(.first_sec) sec1_end = .;} \
# RUN:   .sec2 0x8020 : AT(0x8800) { sec2_start = .; *(.second_sec) sec2_end = .;} \
# RUN: }" > %t-vaddr.script
# RUN: not %link %linkopts -z max-page-size=0x1000 -o /dev/null --script %t-vaddr.script %t.o -shared 2>&1 | FileCheck %s -check-prefix VADDR-OVERLAP-ERR
# VADDR-OVERLAP-ERR: Error: section .sec1 virtual address range overlaps with .sec2
# VADDR-OVERLAP-ERR-NEXT: >>> .sec1 range is [0x8000, 0x80FF]
# VADDR-OVERLAP-ERR-NEXT: >>> .sec2 range is [0x8020, 0x811F]

# Check that the expected binary was created with --no-check-sections
# RUN: %link %linkopts -z max-page-size=0x1000 -o %t.so --script %t-vaddr.script %t.o -shared --no-check-sections
# RUN: llvm-readelf --sections -l %t.so | FileCheck %s -check-prefix BAD-VADDR
# BAD-VADDR-LABEL: Section Headers:
# BAD_VADDR:  .sec1             PROGBITS        {{[0]+}}8000 002000 000100 00  WA  0   0  1
# BAD_VADDR:  .sec2             PROGBITS        {{[0]+}}8020 003020 000100 00  WA  0   0  1
# BAD-VADDR-LABEL: Program Headers:
# BAD_VADDR:  LOAD           0x{{[0]+}}2000 0x{{[0]+}}8000 0x{{[0]+}}8000 0x{{[0]+}}100 0x{{[0]+}}100 RW  0x1000
# BAD_VADDR:  LOAD           0x{{[0]+}}3020 0x{{[0]+}}8020 0x{{[0]+}}8800 0x{{[0]+}}100 0x{{[0]+}}100 RW  0x1000

# Finally check the case where both LMA and vaddr overlap

# RUN: echo "SECTIONS { \
# RUN:   .sec1 0x8000 : { sec1_start = .; *(.first_sec) sec1_end = .;} \
# RUN:   .sec2 0x8040 : { sec2_start = .; *(.second_sec) sec2_end = .;} \
# RUN: }" > %t-both-overlap.script

# RUN: not %link %linkopts -z max-page-size=0x1000 -o /dev/null --script %t-both-overlap.script %t.o -shared 2>&1 | FileCheck %s -check-prefix BOTH-OVERLAP-ERR

# BOTH-OVERLAP-ERR:      Error: section .sec1 virtual address range overlaps with .sec2
# BOTH-OVERLAP-ERR-NEXT: >>> .sec1 range is [0x8000, 0x80FF]
# BOTH-OVERLAP-ERR-NEXT: >>> .sec2 range is [0x8040, 0x813F]
# BOTH-OVERLAP-ERR:      Error: section .sec1 load address range overlaps with .sec2
# BOTH-OVERLAP-ERR-NEXT: >>> .sec1 range is [0x8000, 0x80FF]
# BOTH-OVERLAP-ERR-NEXT: >>> .sec2 range is [0x8040, 0x813F]

.section        .first_sec,"aw"
.rept 0x100
.byte 1
.endr

.section        .second_sec,"aw"
.rept 0x100
.byte 2
.endr
