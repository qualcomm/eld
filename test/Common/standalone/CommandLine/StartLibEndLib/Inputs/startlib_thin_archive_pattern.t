SECTIONS
{
  .foo_out : { "*<start-lib-thin:1>:*foo_sel.o"(.text.foo_sel) }
  .bar_out : { "*<start-lib-thin:1>:*bar_sel.o"(.text.bar_sel) }
  .text : { *(.text*) }
}

