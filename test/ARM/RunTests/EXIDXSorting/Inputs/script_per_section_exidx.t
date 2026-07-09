/* ARM Linux static linker script for EXIDXSortingPerSectionScript RunTest.
 * Base address 0x08048000 matches ARMGNUInfo defaultTextSegmentAddr for
 * ARM Linux static executables (see ARMInfo.h startAddr()).
 * -z max-page-size=0x1000 is required at link time because ARMInfo.h
 * returns abiPageSize=4 when a SECTIONS command is present.
 *
 * The four test-function objects (*.exidx_sort_f_*.o) are placed in a
 * separate .testtext section AFTER .text via EXCLUDE_FILE so that all
 * CRT/libstdc++ code occupies lower virtual addresses.  This keeps the
 * EXIDX catch-all entries at lower addresses than the four per-section
 * test-function entries.
 *
 * Inside .ARM.exidx the catch-all (CRT) rule appears FIRST, followed by
 * the four per-section rules in REVERSE address order (level3->level2->
 * level1->main).  sortEXIDX() reorders these rules to ascending function-
 * address order so the ARM unwinder can binary-search the table.
 *
 * __exidx_start / __exidx_end are defined here because ELD skips its own
 * backend symbol creation when a linker script SECTIONS command is present
 * (ARMLDBackend.cpp initTargetSymbols()).
 */
SECTIONS {
  . = 0x08048000;
  PROVIDE (__ehdr_start = .);
  . = 0x08048000 + SIZEOF_HEADERS;
  .init           : { KEEP(*(.init)) }
  .plt            : { *(.plt) }
  .text : {
    *(EXCLUDE_FILE(*exidx_sort_f_main* *exidx_sort_f_level1*
                   *exidx_sort_f_level2* *exidx_sort_f_level3*) .text*)
  }
  .fini           : { KEEP(*(.fini)) }
  /* Test functions placed after all CRT/libstdc++ text so their virtual
   * addresses are higher.  This ensures the EXIDX catch-all rule's minimum
   * sort key (from CRT) is lower than every per-section test-function key,
   * so sortEXIDX() correctly places the catch-all rule first. */
  .testtext : {
    *(.text.main)
    *(.text._Z6level1v)
    *(.text._Z6level2v)
    *(.text._Z6level3v)
  }
  .rodata         : { *(.rodata*) }
  .eh_frame_hdr   : { *(.eh_frame_hdr) }
  .eh_frame       : { KEEP(*(.eh_frame)) }
  .ARM.extab      : { *(.ARM.extab*) }
  /* EXIDX layout:
   *   1. Catch-all for CRT/libstdc++ (lowest addresses) - FIRST.
   *   2. Per-section rules for test functions in REVERSE address order.
   *      sortEXIDX() reorders these four rules to ascending order. */
  .ARM.exidx : {
    *(EXCLUDE_FILE(*exidx_sort_f_main* *exidx_sort_f_level1*
                   *exidx_sort_f_level2* *exidx_sort_f_level3*) .ARM.exidx*)
    *(.ARM.exidx.text._Z6level3v)
    *(.ARM.exidx.text._Z6level2v)
    *(.ARM.exidx.text._Z6level1v)
    *(.ARM.exidx.text.main)
  }
  PROVIDE_HIDDEN(__exidx_start = ADDR(.ARM.exidx));
  PROVIDE_HIDDEN(__exidx_end = ADDR(.ARM.exidx) + SIZEOF(.ARM.exidx));
  .tdata          : { *(.tdata*) }
  .tbss           : { *(.tbss*) }
  .preinit_array  : {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  }
  .init_array     : {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    PROVIDE_HIDDEN (__init_array_end = .);
  }
  .fini_array     : {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  }
  .ctors          : {
    KEEP(*crtbegin.o(.ctors))
    KEEP(*crtbegin?.o(.ctors))
    KEEP(*(EXCLUDE_FILE(*crtend.o *crtend?.o) .ctors))
    KEEP(*(SORT(.ctors.*)))
    KEEP(*(.ctors))
  }
  .dtors          : {
    KEEP(*crtbegin.o(.dtors))
    KEEP(*crtbegin?.o(.dtors))
    KEEP(*(EXCLUDE_FILE(*crtend.o *crtend?.o) .dtors))
    KEEP(*(SORT(.dtors.*)))
    KEEP(*(.dtors))
  }
  .data.rel.ro.local : { *(.data.rel.ro.local*) }
  .data.rel.ro    : { *(.data.rel.ro*) }
  .dynamic        : { *(.dynamic) }
  .got            : { *(.got) *(.igot) }
  .got.plt        : { *(.got.plt) *(.igot.plt) }
  .data           : { *(.data*) }
  __bss_start = .;
  .bss            : { *(.bss*) *(COMMON) }
  _end = .;
}
