Image Structure and Generation
==============================

.. contents::
   :local:

Processor-specific flags
------------------------

Processor-specific flags are stored in the ``e_flags`` field of the ELF header. ``eld`` will set these flags according to the emulation specified or inferred from its inputs. The following are special cases for the ``e_flags`` field:

1. A lone empty object file::

      touch empty.o
      ld.eld empty.o -m<emulation>
      llvm-readelf -h a.out

   The output file will have flags set according to the emulation specified.

2. A symbol definition file::

      ld.eld -m<emulation> symbols.def

   The output file will have flags set according to the emulation specified.

3. A lone binary file::

      ld.eld -m<emulation> --format=binary foo.bin
      llvm-readelf -h a.out

   Flags are always 0 regardless of the emulation specified.
