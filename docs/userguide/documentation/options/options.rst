Command-line options
======================

Here's the list of command-line options accepted by the ELD:

Common options for all the backends
-------------------------------------

.. include:: GnuLinkerOptions.rst

ARM and AArch64 specific options
---------------------------------

.. include:: ARMLinkerOptions.rst

Hexagon specific options
-------------------------

.. include:: HexagonLinkerOptions.rst

RISCV specific options
-----------------------

.. include:: RISCVLinkerOptions.rst

-z keyword options
-------------------

The following ``-z`` keywords are supported by ELD:

``-z combreloc``
  Combine multiple dynamic relocation sections and sort them to improve dynamic
  symbol lookup caching.

``-z nocombreloc``
  Disable relocation section combining.

``-z global``
  When building a shared object, make its symbols available for resolution by
  subsequently loaded libraries.

``-z defs``
  Report unresolved symbol references from regular object files, even when
  creating a non-symbolic shared library.

``-z initfirst``
  When building a shared object, mark it to be initialized before other objects
  loaded at the same time, and finalized after them.

``-z muldefs``
  Allow multiple definitions.

``-z nocopyreloc``
  Disable linker-generated .dynbss variables used in place of shared-library
  variables. This may result in dynamic text relocations.

``-z nodefaultlib``
  Ignore default library search paths when resolving dependencies at runtime.

``-z relro``
  Create a PT_GNU_RELRO segment and request it be made read-only after
  relocation, if supported. This is disabled by ``-z norelro``.

``-z norelro``
  Do not create a PT_GNU_RELRO segment.

``-z lazy``
  Mark the output for lazy binding (resolve function calls on first use).

``-z now``
  Mark the output for immediate binding (resolve all symbols at load time).

``-z origin``
  Require ``$ORIGIN`` handling in rpath/runpath entries.

``-z text``
  Report an error if DT_TEXTREL is set (dynamic relocations in read-only
  sections).

``-z notext``
  Allow dynamic relocations in read-only sections (do not report DT_TEXTREL).

``-z noexecstack``
  Mark the output as not requiring an executable stack.

``-z nognustack``
  Ignore ``.note.GNU-stack`` and do not create PT_GNU_STACK based on it.

``-z separate-code``
  Create a separate code PT_LOAD segment with instructions on pages disjoint
  from any other data. This is a no-op when a linker script with a ``SECTIONS``
  command is used.

``-z noseparate-code``
  Disable separate code segment handling. This is the default behavior.

``-z execstack``
  Mark the output as requiring an executable stack.

``-z nodelete``
  Mark a shared object as non-unloadable at runtime.

``-z compactdyn``
  Emit a more compact dynamic section by omitting DT_PLTGOT and DT_DEBUG entries.

``-z force-bti``
  Force GNU property BTI feature bits to be recorded (AArch64).

``-z pac-plt``
  Force GNU property PAC feature bits to be recorded for PLT use (AArch64).

``-z common-page-size=<value>``
  Set the most common page size used for segment alignment and layout
  optimization.

``-z max-page-size=<value>``
  Set the maximum supported page size used for segment alignment.

Linker version directive
------------------------

``--enable-linker-version``
  Enable the GNU linker-script ``LINKER_VERSION`` directive. When this option
  is active, every ``LINKER_VERSION`` statement encountered in a linker script
  emits a NUL-terminated string containing the eld version at that position in
  the output section. This matches the GNU ld directive and is useful for
  embedding the linker version directly into a binary. The option applies to
  the entire link once specified.

``--disable-linker-version``
  Disable the ``LINKER_VERSION`` directive. This is the default behaviour, so
  the directive is parsed but emits no data unless the feature has been
  explicitly enabled.

Record command line
-------------------

``--record-command-line``
  Record the linker command line in the ``.comment`` section. This is disabled
  by default.

``--no-record-command-line``
  Do not record the linker command line in the ``.comment`` section.
