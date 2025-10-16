u = 0x100;
PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}
SECTIONS {
  v = u;
  .text : { *(.text*) } :text
  u = 0x300;
  .data : { *(.data*) } :data
}
