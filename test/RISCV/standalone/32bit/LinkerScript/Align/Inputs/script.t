ASSERT(SIZEOF(.text) <= 0x34, "Text section too big!");
SECTIONS {
  .text : { *(.text) *(.text*) }
  ASSERT(SIZEOF(.text) <= 0x34, "Text section too big!");
  .data : {
    ASSERT(SIZEOF(.text) <= 0x34, "Text section too big!");
  }
  ASSERT(SIZEOF(.text) <= 0x34, "Text section too big!");
}
ASSERT(SIZEOF(.text) <= 0x34, "Text section too big!");
