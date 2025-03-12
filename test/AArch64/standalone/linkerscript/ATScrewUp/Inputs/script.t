SECTIONS { 
  .text : { *(.text) }
  __tcm_qurt_pa_load_start__ = 4K;
  __tcm_qurt_pa_run_start__ = 0xd8000000;
  __tcm_va_start__ = 0xd8000000;
  . = __tcm_va_start__;
  __va_start__ = .;

  .tcm_qurt : AT (__tcm_qurt_pa_load_start__ + __va_start__ - __tcm_va_start__)
  { 
    /**(.QURTK.*) */
    *(QURTK.SCHEDULER.text)
    *(QURTK.USER_LIB.text
    QURTK.SCHEDULER.data 
    QURTK.VECTOR.data 
    QURTK.FUTEX.data 
    QURTK.INTERRUPT.data 
    QURTK.CONTEXTS.data 
    )
    /**(QURTK.*) */
  }
  . = ALIGN (8);
  __tcm_qurt_pa_load_end__ = __tcm_qurt_pa_load_start__ + . - __tcm_va_start__;
  .blah : { 
    *(.blah)
  }
}
