# REQUIRES: 32BitsArch

## A writable output section may legally receive a dynamic relocation under
## -z text even if one of the input sections comprising it is read-only.

# RUN: split-file %s %t
# RUN: %clang %clangopts -c %t/1.s -o %t.o -fPIC
# RUN: %link %linkopts -shared -z text -T %t/script.t %t.o -o %t.out
# RUN: %readelf -r %t.out | %filecheck %s --check-prefix=DYNR
# RUN: %readelf -d %t.out | %filecheck %s --check-prefix=NOTEXTREL

# DYNR: Relocation section '.rel{{a?}}.dyn'
# NOTEXTREL-NOT: TEXTREL

#--- 1.s
.section .foo, "a"
.globl foo
foo:
  .word bar

.data
.hidden bar
bar:
  .word 0

#--- script.t
SECTIONS {
  .t : {
    *(.foo)
    *(.data)
  }
}
