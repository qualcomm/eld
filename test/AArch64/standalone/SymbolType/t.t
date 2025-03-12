PHDRS {
  S1 PT_LOAD;
  S2 PT_LOAD;
  S3 PT_LOAD;
}

SECTIONS
{
  S1:
  {
    S1$$Base = . ;
    *(.text .stub .text.*)
  } : S1

  S2:
  {
    S2$$Base = . ;
    *(.data .data.* .data1)
  } : S2
  S2$$Length = SIZEOF(S2);

  S3:
  {
    S3$$Base = . ;
    . = . + 0x00000400;
  } : S3
  S3$$Length = SIZEOF(S3);
}