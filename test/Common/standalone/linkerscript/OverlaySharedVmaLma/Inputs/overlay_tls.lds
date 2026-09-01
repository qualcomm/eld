SECTIONS
{
  OVERLAY 0x2000 : AT(0x0)
  {
    .small { *(.small) }
    .large { *(.large) }
  }

  . = 0x2200;

  .tdata : { *(.tdata) }
  .tbss : { *(.tbss) }
}
