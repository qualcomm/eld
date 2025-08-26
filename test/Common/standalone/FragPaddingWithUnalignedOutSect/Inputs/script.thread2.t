PHDRS {
  ram PT_LOAD;
  tls PT_TLS;
}

MEMORY {
  RAM : ORIGIN = 0x1000, LENGTH = 0x1000
}

SECTIONS {
  .text : { *(.text*) *(*text*) } >RAM AT>RAM :ram

  .tbss (0x1014) (NOLOAD) : {
    . = . + 0x2;
		*(.tbss*)
		*(.tcommon)
	} >RAM AT>RAM :tls :ram

  .tdata (0x2014) : {
    . = . + 0x2;
    *(.tdata*)
  } >RAM AT>RAM :tls :ram
}