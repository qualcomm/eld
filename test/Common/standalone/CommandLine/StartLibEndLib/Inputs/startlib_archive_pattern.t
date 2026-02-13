SECTIONS
{
  .foo_out : { "*<start-lib:1>:*foo_sel.o"(.text.foo_sel) }
  .bar_out : { "*<start-lib:1>:*bar_sel.o"(.text.bar_sel) }
  .text : { *(.text*) }
}

