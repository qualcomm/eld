SECTIONS {
  .text 0x10000 : {
    *(.text.claimed) *(.text.other) *(.text.near) *(.text.near_target)
  }
  .far 0x1000000 : { *(.text.far) }
}
