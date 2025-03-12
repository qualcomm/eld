PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}
SECTIONS
{
  .text           :
  {
    *(.text.main)
    . = ALIGN(4);
  } :A
  .bug : {
  . = ALIGN(16);
  } :A
  . = 0x08000000;
  .text.far :
  {
    *(.text.far*)
  } :B
}
