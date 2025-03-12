
PHDRS {
  SEG1 PT_LOAD;
  SEG2 PT_LOAD;
  SEG3 PT_LOAD;
}
SECTIONS {
    .text : {
      *(.text)
      . = ALIGN(8);
     } :SEG1
    .empty0 : {
       . = . + 100;
     } :SEG2
    .empty1 : {
       . = . + 100;
     } :SEG2
   .CODE : {
      val = .;
    } :SEG3
  .data : { *(.data.*) } :SEG3
  .comment 0 : { *(.comment) }
}
