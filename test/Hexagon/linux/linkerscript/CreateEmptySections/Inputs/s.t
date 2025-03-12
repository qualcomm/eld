SECTIONS {
    .text : {
      *(.text)
      . = ALIGN(8);
     }
    .empty0 : {
       . = . + 100;
     }
    .empty1 : {
       . = . + 100;
     }
   .CODE : {
      val = .;
    }
  .data : { *(.data.*) }
  .comment : { *(.comment) }
}
