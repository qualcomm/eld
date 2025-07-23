PHDRS {
  text PT_LOAD;
  ram_init PT_LOAD;
  tls_init PT_TLS;
}

MEMORY {
  flash (rx!w) : ORIGIN = 0x1000, LENGTH = 0x1000
  ram (w!rx) : ORIGIN = 0x5000, LENGTH = 0x1000
}

SECTIONS {
  .text : {
    *(.text)
    /* assuming .text is going to be 0x20 bytes, add 4 so .text
       is an even multiple of 4 but not 8 */
    . = . + 4;
  } >flash AT>flash :text
  .data : ALIGN_WITH_INPUT {
    *(.data .data.*)
  } >ram AT>flash :ram_init
  PROVIDE(__data_start = ADDR(.data));
  PROVIDE(__data_source = LOADADDR(.data));

  .tdata : ALIGN_WITH_INPUT {
    *(.tdata .tdata.* .gnu.linkonce.td.*)
    PROVIDE(__data_end = .);
    PROVIDE(__tdata_end = .);
  } >ram AT>flash :tls_init :ram_init
  PROVIDE( __tdata_source_end = LOADADDR(.tdata) + SIZEOF(.tdata) );
  PROVIDE( __data_source_end = __tdata_source_end );
  PROVIDE( __data_size = __data_end - __data_start );
  PROVIDE( __data_source_size = __data_source_end - __data_source );
  /DISCARD/ : { *(.ARM.exidx*) *(.hexagon.attributes) *(.riscv.attributes) }
}

ASSERT( __data_size == __data_source_size,
        "ERROR: .data/.tdata flash size does not match RAM size");
