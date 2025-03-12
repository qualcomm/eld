
PHDRS {
  MYSEGA PT_LOAD;
  MYSEGB PT_LOAD;
  compress PT_LOAD;
  dyreclaim PT_LOAD;
}

SECTIONS {
    . = 0xd0000000;
    .mycompress :
      {
        __swapped_segments_start__ = .;
      } :compress

    . = 0xd0000000;
    .reclaim :
      {
        . = . + 100;
      } :dyreclaim

   . = 0;
  .text : { *(.text) } :MYSEGA
  . = ALIGN(4096) + (. & (4096 - 1));
  . = ALIGN(4K);
  .DATA : {} :MYSEGB
  . = ALIGN(8K);
  .DATA1 : {} :MYSEGB
  . = ALIGN(16K);
  .DATA2 : {} :MYSEGB
  . = ALIGN(32K);
  .DATA3 : {} :MYSEGB
  .data : { *.(.data) }
  .bss : { *.(.bss) }
  .comment 0 : { *(.comment) }
}
