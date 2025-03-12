
SECTIONS {
    . = 0xd0000000;
    .mycompress :
      {
        __swapped_segments_start__ = .;
      }

    . = 0xd0000000;
    .reclaim :
      {
        . = . + 100;
      }

   . = 0;
  .text : { *(.text) }
  . = ALIGN(4K);
  .DATA : {}
  . = ALIGN(8K);
  .DATA1 : {}
  . = ALIGN(16K);
  .DATA2 : {}
  .data : { *.(.data) }
  .bss : { *.(.bss) }
  .comment 0 : { *(.comment) }
}
