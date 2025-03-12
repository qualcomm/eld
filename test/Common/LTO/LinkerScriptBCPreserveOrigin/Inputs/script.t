SECTIONS
{
.text_lsy : { *(.text.lsy.*) }
.text_the_rest : { *(.text*) }
.data : { *(.data) }
.bc_bss : { *1*(COMMON) }
.bss : { *(.bss) }
.sdata : { *(.sdata*) }
}
