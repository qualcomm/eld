SECTIONS {
  text : {
            *(.text.foo)
            *(.text.bar)
            *(.text.main)
         }
  .data : {
             *(.data*)
          }
}
