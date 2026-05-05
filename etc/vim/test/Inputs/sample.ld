/* ELD linker script for testing syntax highlighting */
OUTPUT_FORMAT("elf32-littlehexagon", "elf32-bighexagon", "elf32-littlehexagon")
OUTPUT_ARCH(hexagon)
ENTRY(_start)

SEARCH_DIR("/usr/lib")
INCLUDE extra.ld

MEMORY {
  ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00100000
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00080000
}

SECTIONS {
  .text 0x1000 : {
    KEEP (*(.init))
    KEEP (*(.fini))
    *(.text .text.*)
    SORT_BY_NAME(*(.rodata.*))
    EXCLUDE_FILE (libfoo.a) *(.text.foo)
    SORT(CONSTRUCTORS)
  } > ROM

  .bss (NOLOAD) : {
    *(.bss .bss.*)
    *(COMMON)
  } > RAM

  .data : AT (0x2000) {
    *(.data .data.*)
  } > RAM

  /DISCARD/ : {
    *(.comment)
    *(.note.*)
  }
}

. = ALIGN(4);
__heap_start = .;
__stack_size = 0x1000;
PROVIDE_HIDDEN (__stack_end = __heap_start + __stack_size);
PROVIDE (end = .);

PHDRS {
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
  stack PT_GNU_STACK FLAGS(6);
}

/* ── Additional keywords for comprehensive testing ── */

/* eldKwTopLevel: EXTERN VERSION NOCROSSREFS REGION_ALIAS INSERT PRINT
                  LINKER_PLUGIN PLUGIN_CONTROL_FILESZ PLUGIN_CONTROL_MEMSZ
                  PLUGIN_ITER_SECTIONS PLUGIN_OUTPUT_SECTION_ITER
                  PLUGIN_SECTION_MATCHER INCLUDE_OPTIONAL DONTMOVE */
EXTERN(foo_sym bar_sym)
VERSION { v1 { global: *; }; }
NOCROSSREFS(.text .data)
REGION_ALIAS("FLASH", ROM)
INSERT AFTER .text
PRINT("linking done")
LINKER_PLUGIN(myplugin.so)
PLUGIN_CONTROL_FILESZ(0x100)
PLUGIN_CONTROL_MEMSZ(0x200)
PLUGIN_ITER_SECTIONS(iter_fn)
PLUGIN_OUTPUT_SECTION_ITER(out_fn)
PLUGIN_SECTION_MATCHER(match_fn)
INCLUDE_OPTIONAL maybe.ld
DONTMOVE(.keep_me)

/* eldKwInput: AS_NEEDED INPUT GROUP */
INPUT(libfoo.a)
GROUP(libbar.a libbaz.a)
AS_NEEDED(libopt.a)

/* eldKwAssign: HIDDEN ASSERT */
HIDDEN(__hidden_sym = 0x1000);
ASSERT(SIZEOF(.text) > 0, "text must be non-empty");

/* eldKwExpr: all expression built-in functions */
sym_alignof   = ALIGNOF(.text);
sym_addr      = ADDR(.text);
sym_loadaddr  = LOADADDR(.text);
sym_sizeof    = SIZEOF(.text);
sym_soh       = SIZEOF_HEADERS;
sym_defined   = DEFINED(foo_sym);
sym_absolute  = ABSOLUTE(0x1000);
sym_next      = NEXT(0x10);
sym_max       = MAX(0x100, 0x200);
sym_min       = MIN(0x100, 0x200);
sym_log2      = LOG2CEIL(0x1000);
sym_segstart  = SEGMENT_START("text", 0);
sym_origin    = ORIGIN(ROM);
sym_length    = LENGTH(ROM);
sym_dsa       = DATA_SEGMENT_ALIGN(0x1000, 0x200);
sym_dse       = DATA_SEGMENT_END(0x8000);
sym_dsrre     = DATA_SEGMENT_RELRO_END(0, 0x4000);
sym_const     = CONSTANT(COMMONPAGESIZE);
sym_maxpg     = CONSTANT(MAXPAGESIZE);
sym_nextsec   = NEXT_SECTION;
sym_lver      = LINKER_VERSION;

/* eldKwSecType: DSECT COPY INFO OVERLAY PROGBITS UNINIT
                 ONLY_IF_RO ONLY_IF_RW ALIGN_WITH_INPUT SUBALIGN FILL FLAGS */
SECTIONS {
  .dsect_sec (DSECT) : { *(.dsect) }
  .copy_sec  (COPY)  : { *(.copy) }
  .info_sec  (INFO)  : { *(.info) }
  .ovl_sec   (OVERLAY) : { *(.ovl) }
  .prog_sec  (PROGBITS) : { *(.prog) }
  .uninit    (UNINIT) : { *(.uninit) }
  .ro_only   (ONLY_IF_RO) : { *(.ro) }
  .rw_only   (ONLY_IF_RW) : { *(.rw) }
  .aligned   ALIGN_WITH_INPUT : { *(.aligned) }
  .subalign  : SUBALIGN(4) { *(.sub) }
  .filled    : FILL(0xdeadbeef) { *(.fill) }
  .flagged   : FLAGS(SHF_ALLOC) { *(.flagged) }
}

/* eldKwData: BYTE SHORT LONG QUAD SQUAD */
SECTIONS {
  .data_cmds : {
    BYTE(0x01)
    SHORT(0x0102)
    LONG(0x01020304)
    QUAD(0x0102030405060708)
    SQUAD(-1)
  }
}

/* eldKwGroup: START GROUP / END GROUP */
START GROUP
  libfoo.a
  libbar.a
END GROUP

/* eldKwSegType: all PT_* names (no FLAGS suffix so eldKwSegType fires) */
PHDRS {
  p_null    PT_NULL
  p_load    PT_LOAD
  p_dyn     PT_DYNAMIC
  p_interp  PT_INTERP
  p_note    PT_NOTE
  p_shlib   PT_SHLIB
  p_phdr    PT_PHDR
  p_tls     PT_TLS
  p_eh      PT_GNU_EH_FRAME
  p_stack   PT_GNU_STACK
  p_relro   PT_GNU_RELRO
  p_exidx   PT_ARM_EXIDX
}

/* eldKwSectionList: KEEP_DONTMOVE SORT_BY_NAME SORT_BY_ALIGNMENT
                     SORT_BY_INIT_PRIORITY SORT_NONE
   Note: these fire as eldKwSectionList when inside *(keyword(...)) patterns,
   and KEEP_DONTMOVE fires as eldRuleKw when used as a rule prefix with a space. */
SECTIONS {
  .sorted : {
    *(SORT_BY_NAME(.sorted_name.*))
    *(SORT_BY_ALIGNMENT(.sorted_align.*))
    *(SORT_BY_INIT_PRIORITY(.init_prio.*))
    *(SORT_NONE(.unsorted.*))
    KEEP_DONTMOVE (*(.keep_fixed))
  }
}

/* Additional PT_* names from /usr/include/elf.h */
PHDRS {
  p_gnu_prop   PT_GNU_PROPERTY
  p_gnu_sf     PT_GNU_SFRAME
  p_hios       PT_HIOS
  p_hiproc     PT_HIPROC
  p_hisunw     PT_HISUNW
  p_loos       PT_LOOS
  p_loproc     PT_LOPROC
  p_losunw     PT_LOSUNW
  p_mips_abi   PT_MIPS_ABIFLAGS
  p_mips_opt   PT_MIPS_OPTIONS
  p_mips_reg   PT_MIPS_REGINFO
  p_mips_rtp   PT_MIPS_RTPROC
  p_num        PT_NUM
  p_par_arch   PT_PARISC_ARCHEXT
  p_par_unw    PT_PARISC_UNWIND
  p_sunwbss    PT_SUNWBSS
  p_sunwstack  PT_SUNWSTACK
}
