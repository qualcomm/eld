SECTIONS
{
  .foo : { *(.text.foo) }
  .bar: { *(.text.bar) }
  .baz (0xFFFFFF0000000000) : AT (ADDR(.baz)) { *(.text.baz) }
}
