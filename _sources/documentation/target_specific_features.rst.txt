Target Specific Features
==========================

.. contents::
   :local:


Hexagon
-------

- Plugins
  - Information can be found at :doc:`linker_plugins_updated`
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
