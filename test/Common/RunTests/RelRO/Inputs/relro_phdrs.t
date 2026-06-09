/* Minimal PHDRS script for the RelROPHDRS runtime test.
 *
 * Rather than prescribing the full section layout, this script only names
 * the program headers explicitly and lets ELD place sections normally.
 * This exercises the PHDRS code path in createProgramHdrs while keeping
 * the binary loadable by the dynamic linker.
 *
 * The key invariant tested: .tbss must NOT appear in PT_GNU_RELRO, and
 * PT_GNU_RELRO's VirtAddr must be set from the first real RELRO section
 * (.init_array/.got), not from .tbss.
 */
PHDRS {
  ph_phdr   PT_PHDR      PHDRS;
  ph_interp PT_INTERP;
  ph_text   PT_LOAD      FILEHDR PHDRS FLAGS(5);
  ph_data   PT_LOAD      FLAGS(6);
  ph_tls    PT_TLS;
  ph_relro  PT_GNU_RELRO;
  ph_note   PT_NOTE;
  ph_dyn    PT_DYNAMIC;
}
SECTIONS {
  .interp     : { *(.interp) }                             :ph_interp :ph_text
  .note.ABI-tag : { *(.note.ABI-tag) }                     :ph_note :ph_text
  .note.gnu.build-id : { *(.note.gnu.build-id) }           :ph_note :ph_text
  .note.eld.run : { *(.note.eld.run) }                     :ph_note :ph_text
  .dynsym     : { *(.dynsym) }                             :ph_text
  .dynstr     : { *(.dynstr) }                             :ph_text
  .gnu.version   : { *(.gnu.version) }                    :ph_text
  .gnu.version_d : { *(.gnu.version_d) }                  :ph_text
  .gnu.version_r : { *(.gnu.version_r) }                  :ph_text
  .gnu.hash   : { *(.gnu.hash) }                           :ph_text
  .rel.dyn    : { *(.rel.dyn) }                            :ph_text
  .rela.dyn   : { *(.rela.dyn) }                           :ph_text
  .rel.plt    : { *(.rel.plt) }                            :ph_text
  .rela.plt   : { *(.rela.plt) }                           :ph_text
  .init       : { *(.init) }                               :ph_text
  .plt        : { *(.plt*) }                               :ph_text
  . = ALIGN(4K);
  .text       : { *(.text*) }                :ph_text
  .fini       : { *(.fini) }                               :ph_text
  .rodata     : { *(.rodata*) }                            :ph_text
  .eh_frame   : { *(.eh_frame*) }                          :ph_text
  .ARM.exidx  : { *(.ARM.exidx*) }                         :ph_text
  . = ALIGN(4K);
  .tdata      : { *(.tdata*) }                             :ph_tls :ph_data
  .tbss       : { *(.tbss*) }                              :ph_tls :ph_data
  .init_array : { KEEP(*(.init_array*)) }    :ph_relro :ph_data
  .fini_array : { KEEP(*(.fini_array*)) }                  :ph_relro :ph_data
  .dynamic    : { *(.dynamic) }                            :ph_relro :ph_data :ph_dyn
  .got        : { *(.got) }                                :ph_relro :ph_data
  . = ALIGN(4K);
  .got.plt    : { *(.got.plt) }              :ph_data
  __global_pointer$ = .;
  .data       : { *(.data*) }                              :ph_data
  .bss        : { *(.bss*) *(COMMON) }                     :ph_data
}
