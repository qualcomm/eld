MEMORY
{
  RAM (rw) : ORIGIN = 10000, LENGTH = 5000
  ROM (rw) : ORIGIN = 20000, LENGTH = 5000
}

SECTIONS {
 .foo : { *(.text.foo) } > RAM
 .empty : {} > RAM
 tdata : { *(.tdata*) } > ROM
 tbss : { *(.tbss*) } > ROM
 .empty : {} > ROM
 .main : { *(.text.main) } > ROM
 output_data : { BYTE(0x0) *(.data*) } > ROM
 /DISCARD/ : { *(.riscv.attributes) }
}
