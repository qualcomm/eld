MEMORY
{
  RAM (rw) : ORIGIN = 10000, LENGTH = 5000
}

SECTIONS {
 .foo : { *(.text.foo) } > RAM
 .empty : {} > RAM
 tbss : { *(.tbss*) } > RAM
 .empty : {} > RAM
 .main : { *(.text.main) } > RAM
 output_data : { BYTE(0x0) *(.data*) } > RAM
 /DISCARD/ : { *(.riscv.attributes) }
}
