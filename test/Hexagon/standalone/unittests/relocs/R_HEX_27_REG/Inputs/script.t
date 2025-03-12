SECTIONS {
.text : { *(.text*) }
.rodata (0xf000) : {
            _MSG_BASE_ = .;
            . = . + 100;
            *(.rodata*)
          }
}
