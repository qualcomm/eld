SECTIONS {
   .text (0x1233000) : {
    *(.text.test)
   }
   .anothertext (0x1233560) : {
    *(.text.foo)
   }
   .data (0x1234560) : {
     *(.data.*)
   }
}

