ENTRY(_start)

SECTIONS {
  OVERLAY 0x1000 : AT(0x2000) {
    .ov1 { KEEP(*(.text.ov1)) }
    .ov2 { KEEP(*(.text.ov2)) }
  }

  .after : { KEEP(*(.text.after)) }
}
