Target Specific Features
==========================

.. contents::
   :local:


Hexagon
-------

- Plugins
  - Information can be found at :doc:`linker_plugin`
- Small data
- Trampolines
- **--disable-guard-for-weak-undef**
  - Disable guard for weak undefined symbols
- **-gpsize=<value>**
  - Set the maximum size of objects to be optimized using GP


ARM
---

- Baremetal
- Thumb
- ARM
- Linux
- Android
- Supported errata fixes
  - ``fix_cortex_a8``
  - ``fix_cortex_a53_843419``
- **enable-bss-mixing**
  - Enable mixing of BSS and non-BSS sections
- **-execute-only**
  - Mark executable sections execute-only
- **--disable-bss-conversion**
  - Don't convert BSS to NonBSS when BSS/NonBSS sections are mixed
- **--use-mov-veneer**
  - Use movt/movw to load address in veneers with absolute relocation
- **--compact**

  - ``--compact`` is used to reduce the output ELF size.
  - The loader must be able to load such ELF files and satisfy
    ``segmentaddr - sectionaddr == segmentoffset - sectionoffset``.
  - Offsets are incremented by section size, not by virtual address difference.
  - Supported only in ARM/AArch64 baremetal environments (typically low memory).

  .. note::

     The impact of ``--compact`` can be observed by checking the segment sizes.

  .. note::

     - File offset of every section must be consistent with physical address
       of the segment::

         Section_file_offset - segment_file_offset
           == section_physical_address - segment_physical_address

     - If a section does not respect the rule, the linker reports an error.
     - The image must still be a valid ELF; in particular::

         offset % page_alignment
           == physical_address % page_alignment
           == virtual_address % page_alignment

  Example showing ``--compact`` reducing segment size to ``0x20`` as seen from
  ``readelf -l``::

     $ cat script.t
     PHDRS {
       A PT_LOAD;
     }

     SECTIONS {
       .A (0x1000) : AT(0x1000) { *(.text.foo) *(.ARM.exidx.text.foo) } :A
       .B (0x10000) : { *(.text.bar) *(.ARM.exidx.text.bar) } :A
     }

     $ arm-link 1.o -T script.t -o t4.out --compact
     $ llvm-readelf -S -W t4.out
       There are 8 section headers, starting at offset 0x220:

       Section Headers:
       [Nr] Name Type Address Off Size ES Flg Lk Inf Al
       [ 0] NULL      00000000 000000 000000 00 0  0  0
       [ 1] .A PROGBITS 00001000 000054 000010 00 AX 0  0  4
       [ 2] .B PROGBITS 00010000 000064 000010 00 AX 0  0  4

     $ llvm-readelf -l -W t4.out
       Elf file type is EXEC (Executable file)
       Entry point 0x0
       There are 1 program headers, starting at offset 52

       Program Headers:
         Type Offset   VirtAddr   PhysAddr   FileSiz MemSiz Flg Align
         LOAD 0x000054 0x00001000 0x00001000 0x00020 0x0f010 R E 0x4

       Section to Segment mapping:
         Segment Sections...
         00      .A .B
         None    .ARM.attributes .comment .shstrtab .symtab .strtab


AArch64
-------

- Baremetal
- AArch64
- Linux
- Android
- **enable-bss-mixing**
  - Enable mixing of BSS and non-BSS sections
- **--disable-bss-conversion**
  - Don't convert BSS to NonBSS when BSS/NonBSS sections are mixed
- **--use-mov-veneer**
  - Use movt/movw to load address in veneers with absolute relocation
- **--compact**
- **-z nognustack**
  - Do not create a ``GNU_STACK`` segment


RISCV
-----

- Baremetal
- Linux
- **-gpsize=<value>**
  - Set the maximum size of objects to be optimized using GP
- Supports relaxation
  - ``--relax`` (enabled by default)
  - ``-no-relax`` to disable relaxation
- **enable-bss-mixing**
  - Enable mixing of BSS and non-BSS sections
- **-W[no]attribute-mix**
  - Warn about RISC-V attributes mix instead of failing to link
- **--disable-bss-conversion**
  - Don't convert BSS to NonBSS when BSS/NonBSS sections are mixed
