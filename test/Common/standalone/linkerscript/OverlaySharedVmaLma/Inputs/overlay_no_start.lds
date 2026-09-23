ENTRY(_start)

SECTIONS
{
  . = 0x2000;

  OVERLAY : AT(0x0)
  {
    .small { *(.small) }
    .large { *(.large) }
    .tiny { *(.tiny) }
  }

  .after : { *(.after) }
}
