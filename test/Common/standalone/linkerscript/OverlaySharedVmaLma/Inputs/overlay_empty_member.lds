ENTRY(_start)

SECTIONS
{
  . = 0x1000;

  OVERLAY 0x2000 : AT(0x0)
  {
    .small { *(.small) }
    .empty { *(.empty) }
    .large { *(.large) }
  }

  .after : { *(.after) }
}
