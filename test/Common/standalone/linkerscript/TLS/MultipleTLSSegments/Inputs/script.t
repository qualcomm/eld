PHDRS {
  flash PT_LOAD;
  ram_init PT_LOAD;
  ram PT_LOAD;
  tls_init PT_TLS;
  tls PT_TLS;
}

MEMORY {
  flash (rx!w) : ORIGIN = 0x40000000, LENGTH = 0x100
  ram (w!rx)   : ORIGIN = 0x40000200, LENGTH = 0x100
}

SECTIONS {
  .text : { *(.text*) } >flash AT>flash :flash
  .tdata : { *(.tdata*) } >ram AT>flash :tls_init :ram_init
  .tbss (NOLOAD) : { *(.tbss*) } >ram AT>ram :tls :ram
}
