## NOLOAD sections have no physical content in the output file, so their LMA
## range should not be checked for overlap with loaded sections.

# RUN: split-file %s %t
# RUN: %clang %clangopts -c %t/1.s -o %t.o

# RUN: %link %linkopts -T %t/script.t %t.o -o %t.elf

# RUN: %readelf -S %t.elf | %filecheck %s --check-prefix=SECTIONS

# SECTIONS: .bss              NOBITS
# SECTIONS: .noinit           NOBITS
# SECTIONS: foo               PROGBITS

## foo's LMA is aliased to LOADADDR(.bss) via AT(). Since .bss and
## .noinit are NOLOAD, this should not trigger an overlap error.
#--- 1.s
.section .text, "ax"
.globl _start
_start:
    nop

.section .text.foo, "ax"
.globl foo
foo:
    nop

#--- script.t
MEMORY
{
    RAM      (wx) : ORIGIN = 0x8000, LENGTH = 256K
    ONDEMAND (rx) : ORIGIN = 0x8000, LENGTH = 8M
}

SECTIONS
{
    .text : { *(.text) } > RAM
    .bss (NOLOAD) : { BYTE(0xFF) . += 0x100; } > RAM
    .noinit (NOLOAD) : { . += 0x100; } > RAM
    . = ALIGN(0x1000);
    foo (.) : AT(LOADADDR(.bss)) { *(.text.foo) ; . = ALIGN(0x1000); } > ONDEMAND
}
