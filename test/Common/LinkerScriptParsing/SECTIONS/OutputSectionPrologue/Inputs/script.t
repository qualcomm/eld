SECTIONS {
  FOO (0x1000) :
    AT(0x1000)
    ALIGN(0x8)
    SUBALIGN(0x8)
    ONLY_IF_RO
    {
      *(*foo*)
    }

  BAR (0x2000) (PROGBITS) :
    AT(0x2000)
    ALIGN(0x8)
    SUBALIGN(0x8)
    ONLY_IF_RW
    {
      *(*bar*)
    }

  BAZ 0x3000 (PROGBITS, RWX) :
    AT(0x3000)
    ALIGN(0x8)
    SUBALIGN(0x8)
    {
      *(*baz*)
    }
}