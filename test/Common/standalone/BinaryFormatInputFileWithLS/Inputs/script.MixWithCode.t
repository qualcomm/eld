SECTIONS {
  .CodeAndData : { *(.text*) *hello.txt(.data) }
}