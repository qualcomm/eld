# UNSUPPORTED: windows
# REQUIRES: x86

## NEXT_SECTION is only supported as argument to ALIGNOF/SIZEOF.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: %llvm-mc -filetype=obj -triple=x86_64 a.s -o a.o
# RUN: %not %link %linkopts -T script.lds a.o -o /dev/null 2>&1 | FileCheck %s

# CHECK: NEXT_SECTION is only supported as an argument to ALIGNOF or SIZEOF

#--- a.s
.text
.globl _start
_start:
  nop

#--- script.lds
SECTIONS {
  . = 0x1000;
  .text : {
    __bad = ADDR(NEXT_SECTION);
    *(.text)
  }
}

