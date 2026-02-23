# UNSUPPORTED: windows
# REQUIRES: x86

## Test GNU ld's NEXT_SECTION support for ALIGNOF/SIZEOF builtins.
## NEXT_SECTION refers to the next allocated (SHF_ALLOC) output section after
## the current output section being evaluated.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: %llvm-mc -filetype=obj -triple=x86_64 a.s -o a.o
# RUN: %link %linkopts -T script.lds a.o -o out
# RUN: %nm -n out | FileCheck %s

# CHECK: {{^0000000000000000}} {{[Dd]}} __data_next_align
# CHECK: {{^0000000000000000}} {{[Dd]}} __data_next_size
# CHECK: {{^0000000000000010}} {{[TtDd]}} __text_next_align
# CHECK: {{^0000000000000010}} {{[TtDd]}} __text_next_size

#--- a.s
.text
.globl _start
_start:
  nop

.data
  .p2align 4
  .quad 0

.section .debug_info,"",@progbits
  .byte 0

#--- script.lds
SECTIONS {
  . = 0x1000;
  .text : {
    __text_next_align = ALIGNOF(NEXT_SECTION);
    __text_next_size = SIZEOF(NEXT_SECTION);
    *(.text)
  }
  .data : {
    __data_next_align = ALIGNOF(NEXT_SECTION);
    __data_next_size = SIZEOF(NEXT_SECTION);
    *(.data)
  }
  .debug_info : { *(.debug_info) }
}
