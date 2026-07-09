/* Linker script that lists each .ARM.exidx input section as a separate rule
   in reverse address order.  ld.eld must still sort the output by function
   address regardless of the rule ordering in the script. */
SECTIONS {
  .text (0x11000) : {
    *(.text.start) *(.text.f1) *(.text.f2) *(.text.f3)
  }
  .ARM.exidx : {
    *(.ARM.exidx.text.f3)
    *(.ARM.exidx.text.f2)
    *(.ARM.exidx.text.f1)
    *(.ARM.exidx.text.start)
  }
}
