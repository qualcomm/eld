SECTIONS {
  .chroot_text : { /chroot.o(.text) }
  .text : { *(.text) }
}
