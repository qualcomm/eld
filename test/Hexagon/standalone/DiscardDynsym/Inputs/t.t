SECTIONS {
    .text : {
       *(.text*)
     }

    /DISCARD/ : { *(.hash) *(.dynsym) *(.dynstr) }
}

