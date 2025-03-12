SECTIONS {
    .text : {
      *(.text)
      . = ALIGN(8);
     }
    .empty0 : {
       . = . + 100;
       ASSERT(SIZEOF(.empty0) > 0, "blah");
     }
    .empty1 : {
       . = . + 100;
     }
    .CODE (0x80000) : {
     val = .;
    }
    val1 = .;
  .data : { val2 = .; *(.data.*) }
  .comment : { *(.comment) }
}
