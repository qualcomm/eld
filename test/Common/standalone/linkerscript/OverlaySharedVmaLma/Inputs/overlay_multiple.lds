ENTRY(_start)

SECTIONS
{
  . = 0x1000;

  OVERLAY 0x2000 : AT(0x0)
  {
    .small { *(.small) }
  }

  OVERLAY 0x3000 : AT(0x100)
  {
    .large { *(.large) }
    .tiny { *(.tiny) }
  }

  .after : { *(.after) }
}
