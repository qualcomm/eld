ENTRY(_start)

SECTIONS
{
  . = 0x1000;

  OVERLAY 0x2000 :
  {
    .small { *(.small) }
    .large { *(.large) }
    .tiny { *(.tiny) }
  }

  .after : { *(.after) }
}
