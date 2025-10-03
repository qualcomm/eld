MEMORY {
  "RAM REGION" : ORIGIN = 0x1000, LENGTH = 0x3000
}

REGION_ALIAS("RAM ALIAS", "RAM REGION")

SECTIONS {
  .foo (0x1000) : {
    *(.text.foo)
    *(.text)
    *(.ARM.exidx)
  } >"RAM REGION"
  . = 0x2000;
  .bar : { *(.text.bar) } >"RAM ALIAS"
  .data (0x2000) : { *(.data) } >"RAM REGION"
}
