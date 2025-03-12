SECTIONS {
  FOO : { *(.text.foo) }
  BAR (DSECT) : { *(.text.bar) }
  BAZ (COPY) : { *(.text.baz) }
  FRED (INFO) : { *(.text.fred) }
  BOB (OVERLAY) : { *(.text.bob) }
}