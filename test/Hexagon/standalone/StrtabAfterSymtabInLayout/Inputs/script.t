SECTIONS
{
  .text : { *(*text*) }
  .symtab : { *(*symtab*) }
  .strtab : { *(.strtab*) *(.shstrtab*) }
}
