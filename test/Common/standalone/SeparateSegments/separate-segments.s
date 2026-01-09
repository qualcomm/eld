## arm has a different segment layout and alignment with linker scripts than the other targets.
UNSUPPORTED: arm

## Checks -z separate/noseparate-code

# RUN: split-file %s %t
# RUN: %clang -c %t/1.s -o %t.o

# RUN: %link %linkopts %t.o -z separate-code -o %t.out \
# RUN:  --rosegment --no-align-segments -pie
# RUN: %readelf -l %t.out | %filecheck %s

## -z separate-code is an no-op with linker scripts.
# RUN: %link %linkopts %t.o -z separate-code -o %t.script.out \
# RUN:  --rosegment --no-align-segments -pie -T %t/script.t
# RUN: %readelf -l %t.script.out | %filecheck %s --check-prefix=SCRIPT


# CHECK:      LOAD  0x000000 {{.*}} R E 0x1000
# CHECK-NEXT: LOAD  0x001000 {{.*}} R   0x1000
# CHECK-NEXT: LOAD  0x002000 {{.*}} R E 0x1000
## R/RW segments are allowed to overlap
# CHECK-NEXT: LOAD  0x003000 {{.*}} R   0x1000
# CHECK-NEXT: LOAD  0x003{{[0-9a-fA-F]{2}[1-9a-fA-F]}} {{.*}} RW  0x1000

# SCRIPT:      LOAD 0x001{{.*}}                               R
# SCRIPT-NEXT: LOAD 0x001{{[0-9a-fA-F]{2}[1-9a-fA-F]}} {{.*}} R E
# SCRIPT-NEXT: LOAD 0x001{{[0-9a-fA-F]{2}[1-9a-fA-F]}} {{.*}} R
# SCRIPT-NEXT: LOAD 0x001{{[0-9a-fA-F]{2}[1-9a-fA-F]}} {{.*}} RW

#--- script.t
SECTIONS {
  .text   :  { *(.text) }
  .rodata :  { *(.rodata*) }
  .data   :  { *(.data*) }
}

#--- 1.s
.section .data
.byte 0

.section .rodata,"a"
.byte 0

.section .text
.byte 0