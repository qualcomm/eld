SECTIONS {
  .start : { *(.text.start)
            . = . + 0x80000000;
             *(.text.foo) *(.text.car)
           }
}
