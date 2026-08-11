MEMORY {
  ram (rwx) : ORIGIN = 0x1000, LENGTH = 0x10000
}
PHDRS {
  text PT_LOAD;
}
SECTIONS {
  .text : { *(.text*) } > ram :text
}
