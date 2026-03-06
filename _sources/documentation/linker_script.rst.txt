Linker Script
===============
.. contents::
   :local:

Overview
------------

Linker scripts define how input sections (like .text, .data, .bss) are mapped to output sections
and where they are placed in memory. These scripts control:

* Memory regions (e.g., RAM, ROM)
* Section alignment
* Ordering of code and data
* Load vs. execution addresses

This is the standard method used by most linkers like GNU ld.

| Linker scripts provide detailed specifications of how files are to be linked. They offer greater control over linking than is available using just the linker command options.

| NOTE Linker scripts are optional. In most cases, the default behavior of the linker is sufficient.

| Linker scripts control the following properties:

    * ELF program header
    * Program entry point
    * Input and output files and searching paths
    * Section memory placement and runtime
    * Section removal
    * Symbol definition

| A linker script consists of a sequence of commands stored in a text file.

| The script file can be specified on the command line either with -T, or by specifying the file as an input files.

| The linker distinguishes between script files and object files and handles each accordingly.

| To generate a map file that shows how a linker script controlled linking, use the M option.

+---------------+---------------------------------------------------------------------------------+
| Command       | Description                                                                     |
+===============+=================================================================================+
| PHDRS         | Program Headers                                                                 |
+---------------+---------------------------------------------------------------------------------+
| SECTIONS      | Section mapping and memory placementELF program header definition               |
+---------------+---------------------------------------------------------------------------------+
| MEMORY        | Define memory regions used by ``>REGION`` and ``AT>REGION`` placement           |
+---------------+---------------------------------------------------------------------------------+
| ENTRY         | ELF program headerProgram execution entry point                                 |
+---------------+---------------------------------------------------------------------------------+
| OUTPUT_FORMAT | Parsed, but no effect on linking                                                |
+---------------+---------------------------------------------------------------------------------+
| OUTPUT_ARCH   | Parsed, but no effect on linking                                                |
+---------------+---------------------------------------------------------------------------------+
| SEARCH_DIR    |  Add additional searching directory for libraries                               |
+---------------+---------------------------------------------------------------------------------+
| INCLUDE       | Include linker script file                                                      |
+---------------+---------------------------------------------------------------------------------+
| OUTPUT        | Define output filename                                                          |
+---------------+---------------------------------------------------------------------------------+
| GROUP         | Define files that will be searched repeatedly                                   |
+---------------+---------------------------------------------------------------------------------+
| ASSERT        | Linker script assertion                                                         |
+---------------+---------------------------------------------------------------------------------+
| NOCROSSREFS   | Check cross references among a group of sections                                |
+---------------+---------------------------------------------------------------------------------+
| OVERLAY       | GNU ld compatible OVERLAY block (parsing support)                               |
+---------------+---------------------------------------------------------------------------------+

Basic script syntax
--------------------

Symbols
^^^^^^^

Symbol names must begin with a letter, underscore, or period. They can
include letters, numbers, underscores, hyphens, or periods.

Comments
^^^^^^^^

Comments can appear in linker scripts.

Strings
^^^^^^^

Character strings can be specified as parameters with or without
delimiter characters.

Expression basics
^^^^^^^^^^^^^^^^^

Expressions are similar to C, and support all C arithmetic operators.
They are evaluated as type ``long`` or ``unsigned long``.

Location counter basics
^^^^^^^^^^^^^^^^^^^^^^^

A period is used as a symbol to indicate the current location counter.
It is used in the ``SECTIONS`` command only, where it designates
locations in the output section::

  . = ALIGN(0x1000);
  . = . + 0x1000;

Assigning a value to the location counter symbol changes the location
counter to the specified value. The location counter can be moved
forward by arbitrary amounts to create gaps in an output section. It
cannot, however, be moved backwards.

Symbol assignment basics
^^^^^^^^^^^^^^^^^^^^^^^^

Symbols, including the location counter, can be assigned constants or
expressions::

  __text_start = . + 0x1000;

Assignment statements are similar to C, and support all C assignment
operators. Terminate assignment statements with a semicolon.

Script commands
----------------

The ``SECTIONS`` command must be specified in a linker script. All the
other script commands are optional.

PHDRS
^^^^^

The ``PHDRS`` (Program Headers) command in a linker script is used to
define program headers in the output binary, particularly for ELF
Executable and Linkable Format (ELF) files. These headers are essential
for the runtime loader to understand how to load and map the binary into
memory.

*When and why PHDRS is used?*

- Custom memory mapping

  You use ``PHDRS`` when you want to explicitly control how sections are
  grouped into segments in the ELF file. This is especially important
  for:

  - Embedded systems
  - Custom bootloaders
  - OS kernels

- Fine-grained segment control

  - Assign specific sections to specific segments.
  - Control segment flags (for example, ``PT_LOAD``, ``PT_NOTE``,
    ``PT_TLS``).
  - Set permissions (``r``, ``w``, ``x``) for each segment.

Syntax :- ``{ name type [FILEHDR][PHDRS][AT (address)][FLAGS (flags)] }``

The ``PHDRS`` script command sets information in the program headers,
also known as the *segment header* of an ELF output file.

- ``name`` – Specifies the program header in the ``SECTIONS`` command.
- ``type`` – Specifies the program header type.
- ``PT_LOAD`` – Loadable segment.
- ``PT_NULL`` – Linker does not include section in a segment. No
  loadable section should be set to ``PT_NULL``.
- ``PT_DYNAMIC`` – Segment where dynamic linking information is stored.
- ``PT_INTERP`` – Segment where the name of the dynamic linker is
  stored.
- ``PT_NOTE`` – Segment where note information is stored.
- ``PT_SHLIB`` – Reserved program header type.
- ``PT_PHDR`` – Segment where program headers are stored.
- ``FLAGS`` – Specifies the ``p_flags`` field in the program header.
  The value of flags must be an integer. It is used to set the
  ``p_flags`` field of the program header; for instance,
  ``FLAGS(5)`` sets ``p_flags`` to ``PF_R | PF_X`` and
  ``FLAGS(0x03000000)`` sets OS-specific flags.

.. note::
   If the sections in an output file have different flag settings than
   what is specified in ``PHDRS``, the linker combines the two different
   flags using bitwise OR.

.. _linker-script-memory:

MEMORY
^^^^^^^

The ``MEMORY`` command defines named memory regions (for example ``FLASH`` and
``RAM``) that can be used to place output sections without hard-coding absolute
addresses.

In ELD, memory regions can be used in three related ways:

1. **VMA placement**: ``>REGION`` assigns the output section's VMA to the next
   available address in ``REGION`` (starting from ``ORIGIN`` and advancing by
   the output section size).
2. **LMA placement**: ``AT>REGION`` assigns the output section's LMA to the next
   available address in ``REGION`` (tracked independently from VMA placement).
3. **Automatic region selection**: if an allocatable output section does not
   specify ``>REGION``, ELD can select a region by matching the section's flags
   against region attributes (for example, place writable sections in a ``(w)``
   region and code in an ``(x)`` region).

Syntax
""""""

.. code-block::

  MEMORY {
    <name> [(<attrs>)] : ORIGIN = <expr> , LENGTH = <expr>
    <name> [(<attrs>)] : org = <expr> , len = <expr>
    ...
  }

Notes:

- ``ORIGIN`` can also be spelled as ``org`` or ``o``.
- ``LENGTH`` can also be spelled as ``len`` or ``l``.
- ``ORIGIN`` and ``LENGTH`` must be separate tokens (for example,
  ``ORIGIN = 0x1000`` is accepted, but ``ORIGIN= 0x1000`` is rejected).
- The expressions are normal linker-script expressions; you can use constants
  and arithmetic, and size suffixes like ``K``, ``M``, and ``G`` (for example,
  ``LENGTH = 4K``).
- The ``MEMORY`` block can contain ``INCLUDE``/``INCLUDE_OPTIONAL`` directives.

Memory region names
"""""""""""""""""""

Region names are identifiers (and may also appear quoted in scripts). Region
names are referenced in output section descriptions via ``>REGION`` and
``AT>REGION``.

If a section references an unknown region name, ELD errors out (for example,
``Cannot find memory region <name>``).

Memory region attributes
"""""""""""""""""""""""""

``(<attrs>)`` is optional and is primarily used for **automatic region
selection** (when an allocatable output section does not specify an explicit
``>REGION``).

ELD supports the following attribute letters:

- ``w``: writable (matches ``SHF_WRITE`` sections).
- ``x``: executable (matches ``SHF_EXECINSTR`` sections).
- ``a``: allocatable (matches ``SHF_ALLOC`` sections).
- ``r``: read-only (matches *non-writable* allocatable sections; ELF sections do
  not explicitly carry a read flag).
- ``i`` / ``l``: initialized (matches ``SHT_PROGBITS`` sections; used to avoid
  matching ``SHT_NOBITS`` when desired).
- ``!``: negation operator, used to say that attributes must **not** be present.
  For example, ``(r!x)`` matches non-writable, non-executable sections.

The attribute matching rules apply only to allocatable output sections that do
not already have an explicit ``>REGION``.

.. important::

   If you explicitly assign a section to a region using ``>REGION``, ELD does
   not reject the assignment even if the section's flags do not match the
   region's attributes. Attributes are for auto-selection, not enforcement.

Automatic region selection (orphans and missing ``>REGION``)
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

When a linker script uses ``MEMORY`` and an allocatable output section is not
explicitly assigned to a region, ELD attempts to assign it to the first memory
region (in ``MEMORY`` order) whose attributes match the section's flags.

This includes *orphan output sections* (output sections created implicitly
because some input sections were not matched by any rule in ``SECTIONS``).

If no memory region matches an allocatable output section, ELD errors out (for
example, ``Error: No memory region assigned to section .data``).

.. note::

   This behavior can differ from GNU ld and LLD for orphans. For example, some
   linkers may keep orphan sections in the "current" region/cursor even when
   region attributes suggest a different region. ELD follows the region
   attribute matching rules for orphans and other unassigned allocatable output
   sections. See https://github.com/qualcomm/eld/issues/127 for a concrete
   example and background.

Common recommendation: if the placement of a section matters (for example,
``.rodata``, ``.eh_frame``, ``.tbss``), add an explicit output section rule and
assign it to the intended region rather than relying on orphan heuristics.

Interaction with explicit addresses (VMA overrides)
"""""""""""""""""""""""""""""""""""""""""""""""""""

If an output section has an explicit VMA (for example ``.text 0x2000 : { ... }``
or via ``--section-start``), the explicit VMA is honored.

If that output section also has ``>REGION``:

- If the explicit VMA lies **within** the region's ``[ORIGIN, ORIGIN+LENGTH]``
  range, the section is still accounted against the region for memory usage
  checks/reporting.
- If the explicit VMA lies **outside** the region range, the section is not
  charged against that region (the explicit address is treated as an override).

This is intended to support scripts that use regions as a default placement
mechanism while still allowing some sections to be placed at fixed addresses.

TLS and ``.tbss``
""""""""""""""""""

ELD treats TLS ``SHT_NOBITS`` sections (for example ``.tbss``) specially for
``MEMORY`` region cursors: ``.tbss`` does not advance the region cursor, so a
subsequent non-TLS output section may end up with the same VMA. If your script
requires non-overlapping VMAs for TLS and non-TLS sections, place TLS output
sections explicitly (for example, in a dedicated region or at a dedicated
address range) rather than relying on region cursors.

LMA regions and ``AT>REGION``
"""""""""""""""""""""""""""""

Use ``AT>REGION`` on an output section description to place the section's **load
memory address (LMA)** in a region, independent of its VMA placement:

.. code-block::

  .text : { *(.text*) } >RAM AT>FLASH

Rules:

- ``AT(<address>)`` and ``AT>REGION`` are mutually exclusive; specifying both is
  an error.
- If a section has a VMA region (``>REGION``) but no explicit LMA control, ELD
  defaults the LMA region to the same region as the VMA region.
- If a section has explicit LMA control (``AT(<address>)``), ELD does not
  automatically align the LMA. (See :ref:`controlling-physical-addresses`.)

Builtins: ``ORIGIN()`` and ``LENGTH()``
"""""""""""""""""""""""""""""""""""""""

ELD supports GNU-compatible ``ORIGIN(<region>)`` and ``LENGTH(<region>)``
builtins in linker script expressions.

These functions accept a memory region name or a region alias and evaluate to
the region's origin and length, respectively. If the region name cannot be
resolved, ELD errors out.

REGION_ALIAS
""""""""""""

``REGION_ALIAS("alias", "region")`` defines an alias name for an existing memory
region. Aliases can be used anywhere a region name is expected (for example,
``>alias`` or ``ORIGIN(alias)``).

ELD restrictions and diagnostics:

- Creating two aliases with the same alias name is an error.
- An alias name must not collide with a real memory region name.

Memory usage checking and reporting
"""""""""""""""""""""""""""""""""""

When ``MEMORY`` is used, ELD tracks memory usage per region during layout:

- If the total size placed in a region exceeds ``LENGTH``, ELD errors out and
  reports the overflow amount per output section.
- With ``-Wlinker-script-memory``, ELD can warn on zero-length regions and on
  regions that end up with zero used size after layout.

Additionally:

- The text map format (``-MapStyle txt``) includes a ``# MEMORY`` block that
  prints the parsed ``MEMORY`` regions and annotates each output section line
  with ``Memory : [<VMARegion>, <LMARegion>]`` when available.
- ``--print-memory-usage`` prints a summary table of used size and percentage
  per memory region when a ``MEMORY`` directive is present.

PHDRS interaction and program header loading
""""""""""""""""""""""""""""""""""""""""""""

``MEMORY`` controls **where sections go** (VMA via ``>REGION`` and LMA via
``AT>REGION``). ``PHDRS`` controls **which segments exist** and which output
sections are assigned to those segments (via ``:phdr`` on output section
descriptions).

Key points:

- If you do not specify ``PHDRS``, ELD creates segments using its default
  heuristics. A change in memory region (for example, ``>FLASH`` to ``>RAM``)
  can force ELD to start a new ``PT_LOAD`` segment.
- If you specify ``PHDRS``, ELD only creates the program headers declared in
  the script. In this mode, region changes do *not* create extra segments; you
  must assign each output section to the intended segment(s) using ``:phdr``.
- Runtime loaders use the ELF program headers (and sometimes the file header)
  to decide what to map or copy. If your loader needs the file header and/or
  program headers to be part of the first loadable segment, use ``FILEHDR`` and
  ``PHDRS`` keywords on the corresponding ``PT_LOAD`` in the ``PHDRS`` command.

Example (first segment includes file+program headers)::

  PHDRS {
    text PT_LOAD FILEHDR PHDRS;
    data PT_LOAD;
  }

If you include headers in a loadable segment, you typically must also reserve
space for them in the image layout (see :ref:`memory-sizeof-headers`).

.. _memory-sizeof-headers:

Using SIZEOF_HEADERS with MEMORY
"""""""""""""""""""""""""""""""""

``SIZEOF_HEADERS`` evaluates to the total size (in bytes) of the ELF file header
and program header table that ELD emits for the output.

Nuance: in ELD, using ``SIZEOF_HEADERS`` *before the first output section is
seen* also acts as a signal that the script is intentionally accounting for
headers; this impacts whether ELD considers the headers as occupying space at
the start of the image when a linker script is present.

Common MEMORY pitfalls and errors
"""""""""""""""""""""""""""""""""

- **Tokenization**: ``ORIGIN``/``LENGTH`` must be separate tokens (``ORIGIN =
  ...``); scripts like ``ORIGIN= 0x1000`` are rejected.
- **Unknown region name**: ``>REGION`` or ``AT>REGION`` refers to a region not
  defined in ``MEMORY`` (error: ``Cannot find memory region ...``).
- **No region assigned**: if ``MEMORY`` is present and an allocatable output
  section does not have an explicit region and does not match any region
  attributes (error: ``No memory region assigned to section ...``).
- **Overflow**: placed size exceeds ``LENGTH`` (error: ``Memory region <name>
  exceeded limit ... overflowed by ...``). Remember to account for alignment,
  page alignment, and headers if ``FILEHDR PHDRS`` are used.
- **Mixing ``AT(address)`` with ``AT>REGION``**: these are mutually exclusive.
  If you need a fixed LMA base but still want region-based placement for the
  rest of the image, prefer shifting the region origin using expressions (see
  :ref:`memory-sizeof-headers`) or keep all LMAs explicit.
- **Orphan placement surprises**: orphan output sections and other unassigned
  allocatable sections are auto-assigned using region attributes; if a specific
  section must be in a particular region, add an explicit output section rule.

SECTIONS
^^^^^^^^

Syntax :- ``SECTIONS { section_statement section_statement ... }``

The ``SECTIONS`` script command specifies how input sections are mapped
to output sections, and where output sections are located in memory. The
``SECTIONS`` command must be specified once, and only once, in a linker
script.

Section statements
""""""""""""""""""

A ``SECTIONS`` command contains one or more section statements, each of
which can be one of the following:

- An ``ENTRY`` command.
- A symbol assignment statement to set the location counter. The
  location counter specifies the default address in subsequent
  section-mapping statements that do not explicitly specify an address.
- An output section description to specify one or more input sections in
  one or more library files, and map those sections to an output
  section. The virtual memory address of the output section can be
  specified using attribute keywords.

ENTRY
^^^^^

Syntax :- ``ENTRY(symbol)``

- The ``ENTRY`` script command specifies the program execution entry
  point.
- The entry point is the first instruction that is executed after a
  program is loaded.
- This command is equivalent to the linker command-line option
  :option:`-e`.

OUTPUT_FORMAT
^^^^^^^^^^^^^

Syntax :- ``OUTPUT_FORMAT(string)``

- The ``OUTPUT_FORMAT`` script command specifies the output file
  properties.
- For compatibility with the GNU linker, this command is parsed but has
  no effect on linking.

OUTPUT_ARCH
^^^^^^^^^^^

Syntax :- ``OUTPUT_ARCH("aarch64")``

- The ``OUTPUT_ARCH`` script command specifies the target processor
  architecture.
- For compatibility with the GNU linker, this command is parsed but has
  no effect on linking.

SEARCH_DIR
^^^^^^^^^^

Syntax :- ``SEARCH_DIR(path)``

- The ``SEARCH_DIR`` script command adds the specified path to the list
  of paths that the linker uses to search for libraries.
- This command is equivalent to the linker command-line option
  :option:`-L`.

INCLUDE
^^^^^^^

Syntax :- ``INCLUDE(file)``

- The ``INCLUDE`` script command specifies the contents of the text file
  at the current location in the linker script.
- The specified file is searched for in the current directory and any
  directory that the linker uses to search for libraries.

.. note::
   Include files can be nested.

OUTPUT
^^^^^^

Syntax :- ``OUTPUT(file)``

- The ``OUTPUT`` script command defines the location and file where the
  linker will write output data.
- Only one output is allowed per linking.

GROUP
^^^^^

Syntax :- ``GROUP(file, file, …)``

- The ``GROUP`` script command includes a list of archive file names.
- The archive names defined in the list are searched repeatedly until
  all defined references are resolved.

ASSERT
^^^^^^

Syntax :- ``ASSERT(expression, string)``

- The ``ASSERT`` script command adds an assertion to the linker script.


Expressions
------------

Expressions in linker scripts are identical to C expressions

.. list-table:: Expression helper functions
   :widths: 30 70
   :header-rows: 1

   * - Function
     - Description
   * - ``.``
     - Return the location counter value representing the current virtual address.
   * - ``ABSOLUTE(expression)``
     - Return the absolute value of the expression.
   * - ``ADDR(string)``
     - Return the virtual address of the symbol or section. ``.`` is supported.
   * - ``ALIGN(expression)``
     - Return the value of the location counter when aligned to the next expression boundary.
   * - ``ALIGN(expression1, expression2)``
     - Return ``expression1`` rounded up to the next ``expression2`` boundary.
   * - ``ALIGNOF(string)``
     - Return the alignment of the symbol or section. ``NEXT_SECTION`` returns the next allocated output section alignment (only supported with ``ALIGNOF``/``SIZEOF``).
   * - ``ASSERT(expression, string)``
     - Throw an assertion if the expression evaluates to zero.
   * - ``BLOCK(expression)``
     - Synonym for ``ALIGN(expression)``.
   * - ``DATA_SEGMENT_ALIGN(maxpagesize, commonpagesize)``
     - Equivalent to ``ALIGN(maxpagesize) + (. & (maxpagesize - 1))`` or ``ALIGN(maxpagesize) + (. & (maxpagesize - commonpagesize))`` (the larger of the two).
   * - ``DATA_SEGMENT_END(expression)``
     - Return the value of the expression.
   * - ``DATA_SEGMENT_RELRO_END(expression)``
     - Return the value of the expression.
   * - ``DEFINED(symbol)``
     - Return 1 if the symbol is defined in the global symbol table.
   * - ``LOADADDR(string)``
     - Synonym for ``ADDR``.
   * - ``MAX(expression1, expression2)``
     - Return the maximum of two expressions.
   * - ``MIN(expression1, expression2)``
     - Return the minimum of two expressions.
   * - ``SEGMENT_START(string, expression)``
     - If the string matches a known segment, return its start address; otherwise return the expression.
   * - ``SIZEOF(string)``
     - Return the size of the symbol, section, or segment. ``NEXT_SECTION`` returns the size of the next allocated output section (only with ``ALIGNOF``/``SIZEOF``).
   * - ``SIZEOF_HEADERS``
     - Return the total size (bytes) of the ELF file header plus program headers.
   * - ``CONSTANT(MAXPAGESIZE)``
     - Return the ABI-defined maximum page size.
   * - ``CONSTANT(COMMONPAGESIZE)``
     - Return the ABI-defined common page size.

Symbol assignments
---------------------

Linker scripts can define symbols that are used by the link to create the
output image. Linker script symbols can be referenced by the program
source code. One typical use of linker script symbols is to define the size
and start / stop address of an output section.

Linker script symbol assignments support most C operators and follow
C-like rules. For example, all the assignments must end with a semi-colon, and
the C arithmetic operators are supported. The below mathematical operators are supported in linker script
assignments::

  u = v + w;
  u = v - w;
  u = v * w;
  u = v / w;
  u = v & w;
  u = v | w;
  u = v << w;
  u = v >> w;

Additionally, linker script also supports compound assignment operators::

  u += v;
  u -= v;
  u *= v;
  u /= v;
  u &= v;
  u |= v;
  u <<= v;
  u >>= v;

The left-hand side symbol must already be defined when using compound assignment operator.

Symbol assignment types
^^^^^^^^^^^^^^^^^^^^^^^^

Linker script symbol assignments are of 4 key types:

  - :code:`symbol = expression;`

    Defines a :code:`GLOBAL` symbol.

  - :code:`HIDDEN(symbol = expression);`

    Defines a ``GLOBAL`` symbol with ``HIDDEN`` visibility.

  - :code:`PROVIDE(symbol = expression);`

    Defines the :code:`symbol` only if it is required. If defined,
    the symbol will have :code:`GLOBAL` symbol binding.

  - :code:`PROVIDE_HIDDEN(symbol = expression);`

    Defines the :code:`symbol` only if it is required. If defined,
    the symbol will be a ``GLOBAL`` symbol with :code:`HIDDEN` visibility.

Section of linker script symbols
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Linker script symbols defined outside an output section directive are
called *absolute* symbols. That is, they do not belong to any output section.
The section index of such symbols is a sentinel value :code:`SHN_ABS`.

Linker script symbols that are defined within an output section directive
have the output section index as their section index.

.. code-block::

   // script.t
   u = 0x100;  // ABS symbol
   SECTIONS {
     v = 0x300; // ABS symbol
     foo : {
       *(.text.foo)
       w = 0x500;  // Section of the symbol is foo
     }
   }
   e = 0x700; // ABS symbol

Location counter
^^^^^^^^^^^^^^^^^^

:code:`.` symbol is a special linker symbol that always contains the current output
location address. Assigning to it changes the current output
location address and can be used to create holes in the output image. This special
dot symbol is called the location counter. It may also be referred to as the
dot counter and dot symbol.

Assignment evaluation order
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Understanding symbol assignment evaluation order is the key to understanding
linker scripts and a necessary skill to debug linker script related issues.
For the most part, the linker script assignments behave how you expect, but
things become interesting when forward references are involved.

Basic case: No forward reference
""""""""""""""""""""""""""""""""""

Let's start with a basic case that does not have any forward references.

.. code-block::

   u1 = 0x100;  // A1
   SECTIONS {
     u2 = 0x300; // A2
     foo : {
       *(.text.foo)
       u3 = 0x500;  // A3
     }
     u4 = 0x700;  // A4
     bar : {
       u5 = 0x900;  // A5
       *(.text.bar)
     }
     u5 = 0x1100; // A6
   }
   u6 = 0x1300;  // A7

The linker evaluates the assignment during the layout phase. The assignments
, when they do not contain any forward reference, are evaluated in the
script order. Thus, in this case, the linker script assignment evaluation
order is: :code:`[A1, A2, A3, A4, A5, A6, A7]`.

Forward reference
""""""""""""""""""

We will now see a more complex example that has forward references. But before we
dive into the example, let's understand how linker evaluates an individual assignment
that has forward reference.

.. code-block::

   u = v + w;

For the above assignment, let's say that :code:`v` is defined before this assignment as per the linker script
and :code:`w` gets defined later. In such case, the *final* value of the symbol (:code:`w` in this case) is
used to evaluate the assignment. Let's understand this with a more concrete example::

  v = 0x100; // A1
  u = v + w; // A2
  w = 0x200; // A3
  foo = w;   // A4
  w = 0x400; // A5
  v = 0x600; // A6

When A2 (:code:`u = v + w`) is evaluated, :code:`v` is already defined and is thus evaluated
to its current value :code:`0x100`. On the other hand, :code:`w` is not defined when A2 is
executed and thus the final value of :code:`w` (i.e., 0x400) is used to evaluate A2.
Thus, after all assignments are processed, the final values are:

- :code:`v = 0x600`
- :code:`u = 0x500`
- :code:`foo = 0x200`
- :code:`w = 0x400`

Now let's look at a more complex example containing forward references.

.. code-block::

   u = v; // A1
   SECTIONS {
    v = v1; // A2
    foo : {
      *(.text.foo)
      v1 = v2; // A3
    }
    v2 = v3; // A4
   }
   v3 = SIZEOF(foo); // A5

The linker again tries to evaluate the assignment in order, however, the assignments
A1, A2, A3 and A4 cannot be completely evaluated because the variables on their
right hand side are not evaluated yet. The linker marks the assignments that cannot
be completely evaluated as pending assignments. It also records which nodes were unevaluated
in the assignment. After the layout is complete, but before the relaxations begin, the linker
recursivly evaluates the pending assignments until all assignments are resolved or a
circular dependency is encountered. During re-evaluation of an assignment, only the previously
unevaluated nodes are reevaluated.

The assignment evaluation sequence for this example is:

1. Evaluate [A1, A2, A3, A4, A5]: PendingAssignments = [A1, A2, A3, A4], CompletedAssignments = [A5]
2. Re-evaluate [A1, A2, A3, A4]: PendingAssignments = [A1, A2, A3], CompletedAssignments = [A5, A4]
3. Re-evalaute [A1, A2, A3]: PendingAssignments = [A1, A2], CompletedAssignments = [A5, A4, A3]
4. Re-evaluate [A1, A2]: PendingAssignments = [A1], CompletedAssignments = [A5, A4, A3, A2]
5. Re-evaluate [A1]: PendingAssignments = [], CompletedAssignments = [A5, A4, A3, A2, A1]

Circular dependency
""""""""""""""""""""

What happens if the linker script contains circular dependency among variables?

.. code-block::

   u = v + 0x1; // A1
   v = w + 0x1; // A2
   w = u + 0x1; // A3

What would be the values of :code:`u`, :code:`v`, and :code:`w` here?

In such a case, the linker would report a warning and stop evaluating symbol assignments
once a circular dependency is detected. The final values of the symbols here will be:

- :code:`u = 0x2`
- :code:`v = 0x2`
- :code:`w = 0x2`

Before analyzing this behavior, it's important to note that a circular dependency
represents an erroneous condition and should be considered undefined behavior.
Once a layout enters an undefined state, all guarantees regarding its structure
and consistency no longer apply.

With this warning in-place, let's reason how linker arrives at these final values.
The assignment evaluation sequence for this example is:

1. Evaluate [A1, A2, A3]: PendingAssignments = [A1, A2, A3], CompletedAssignments = []
2. Re-evaluate [A1, A2, A3]: PendingAssignments = [A1, A2, A3], CompletedAssignments = [],
   Circular dependency detected, stop evaluation.


The linker stops evaluating assignments when it detects a circular dependency.
The linker assigns the value :code:`0x1`` to the symbols in the first assignment evaluation iteration,
then in the second evaluation iteration it assigns the value :code:`0x2` to the symbols. The linker
then detects circular dependency and does not evaluate assignments further, as the layout may never
converge due to circular dependencies.

Forward references in dot-assignments
""""""""""""""""""""""""""""""""""""""""

Forward references in dot-assignments are more complex than those in non-dot
assignments because dot-assignments directly influence the layout.
If a dot-assignment cannot be evaluated correctly, the layout itself cannot be computed.

The fundamental rule remains unchanged: whenever a forward reference is encountered,
the final value of the symbol is used in the expression.

In eld, when a forward reference appears in a dot-assignment,
the layout is computed in two passes:

- First pass: The forward reference symbol is temporarily treated as 0
  during layout computation.
- Second pass: After the initial layout is complete, eld recomputes the
  layout using the actual final values of all forward reference symbols.

--defsym
^^^^^^^^

``--defsym sym=expr`` is treated akin to a linker script just with
one symbol assignment. For example::

  ld.eld -o 1.out 1.o --defsym u=0x10 --defsym v=0x30 --defsym w=0x50

The above link command is equivalent to::

  # The scripts consist of the following content:
  # script1.t: "u=0x10"
  # script2.t: "v=0x30"
  # script3.t: "w=0x50"
  ld.eld -o 1.out 1.o script1.t script2.t script3.t


.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Function
     - Description
   * - ``HIDDEN(symbol = expression)``
     - Hide the defined symbol so it is not exported.
   * - ``FILL(expression)``
     - Specify a fill pattern for the current output section. The fill element
       size can be 1, 2, 4, or 8 bytes (chosen by the linker). A ``FILL`` covers
       memory locations from the point at which it occurs to the end of the
       current output section; multiple ``FILL`` statements can be used within
       one output section.
   * - ``ASSERT(expression, string)``
     - If the specified expression is zero, the linker errors out with the
       specified message.
   * - ``PROVIDE(symbol = expression)``
     - Similar to a symbol assignment, but does not error if the symbol is
       already defined elsewhere.
   * - ``PROVIDE_HIDDEN(symbol = expression)``
     - Like ``PROVIDE``, but the defined symbol is hidden (not exported).
   * - ``PRINT("format-string", expr, ...)``
     - Print a formatted message while parsing the script. See the
       :ref:`PRINT Command <linker-script-print>` section for format string
       syntax and supported conversions.

NOCROSSREFS
---------------
     * The NOCROSSREFS command takes a list of space-separated output section names as its arguments.

     * Any cross references among these output sections will result in link editor failure.

     * The list can also contain an orphan section that is not specified in the linker script.

     * A linker script can contain multiple NOCROSSREFS commands.

     * Each command is treated as an independent set of output sections that are checked for cross references.

OVERLAY
---------------

In GNU ld linker scripts, the ``OVERLAY`` command is used to define multiple
sections that all execute (run) at the same virtual memory address (VMA), but
are stored (loaded) at different load memory addresses (LMA). At runtime, only
one of those sections is present in memory at a time, and software is
responsible for copying the desired section into the overlay region before
executing it.

What ``OVERLAY`` does in GNU ld
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. Same execution address (VMA)
   All sections inside an ``OVERLAY`` block are linked to start at the same
   address. From the CPU's point of view, they occupy the same memory region.

2. Different load addresses (LMA)
   Each overlaid section has a distinct load address in the output image (often
   in flash or ROM). These LMAs are laid out back-to-back starting at the
   overlay's ``AT(...)`` address.

3. Runtime-managed swapping
   The linker does not generate code to manage overlays. Your runtime code
   (overlay manager) must copy the desired section from its LMA into the overlay
   execution region before calling into it.

Auto-generated symbols in GNU ld
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For each section inside an overlay, GNU ld defines:

* ``__load_start_<section>``
* ``__load_stop_<section>``

These symbols let your code know where to copy from.

``NOCROSSREFS``
^^^^^^^^^^^^^^^

GNU ld accepts an optional ``NOCROSSREFS`` keyword in the overlay header, e.g.::

  OVERLAY 0x1000 : NOCROSSREFS AT(0x4000) { ... }

This causes GNU ld to error out if one overlaid section references another.

Effect on the location counter (``.``) in GNU ld
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

After an ``OVERLAY`` block, ``.`` is advanced by the size of the largest overlay
member. This ensures the overlay region reserves enough execution memory for the
largest case.

Syntax
^^^^^^^

.. code-block::

  OVERLAY [<start>] :
      [NOCROSSREFS] [AT(<lma_start>)]
  {
      <overlay-member>...
  } [><region>] [AT><lma_region>] [:<phdr>...] [=<fillexp>]

  <overlay-member> :=
      <output-section-name> { <input-section-description>... }

.. important::

   Overlay member sections are parsed as *name + body only*. Individual overlay
   members must not use the normal output section description prologue/epilogue
   syntax (for example, no ``:`` prologue, no member-level ``AT(...)``, no
   member-level ``>REGION``/``AT>REGION``, no ``:PHDR`` list, and no
   ``=<fill>``). eld errors out if these constructs are used on overlay members.

eld support status
^^^^^^^^^^^^^^^^^^

eld currently supports parsing ``OVERLAY`` blocks and printing them into the
text map file (``-MapStyle txt``) as comments. The GNU ld overlay *semantics*
described above (LMA/VMA overlay placement, generated symbols, overlay-member
swapping behavior, overlay-specific ``NOCROSSREFS`` enforcement, and location
counter advancement rules) are not implemented yet.

Output Section Description
---------------------------

A ``SECTIONS`` command can contain one or more output section descriptions.

.. code-block::

    <section-name> [<virtual_addr>][(<type>)] :
    [AT(<load_addr>)] [ALIGN(<section_align>) | ALIGN_WITH_INPUT]
    [SUBALIGN(<subsection_align>)] [<constraint>]
    {
       ...
       <output-section-command> <output-section-command>
    }[><region>][AT><lma_region>][:<phdr>...][
    =<fillexp>][INSERT AFTER <section-name> | INSERT BEFORE <section-name>]

Syntax
------

<section-name>
    Specifies the name of the output section.

<virtual_addr>
    Specifies the virtual address of the output section (optional). The address value can be an expression (see Expressions).

<type>
    Specifies the section load property (optional).

    - NOLOAD: Marks a section as not loadable.
    - INFO: Parsed only; has no effect on linking.

<load_addr>
    Specifies the load address of the output section (optional). The address value can be specified as an expression (see Expressions).

<section_align>
    Specifies the section alignment of the output section (optional). The alignment value can be an expression (see Expressions).

<subsection_align>
    Specifies the subsection alignment of the output section (optional). The alignment value can be an expression (see Expressions).

<constraint>
    Specifies the access type of the input sections (optional).

    - NOLOAD: All input sections are read-only.

<output-section-command>
    Specifies an output section command (see Output section commands). An output section description contains one or more output section commands.

<region>
    Specifies the VMA placement memory region (optional) using ``>REGION``.

    If the output section does not have an explicit VMA, ELD uses the next
    available address in the region (starting from ``ORIGIN`` and respecting
    alignment) as the section's VMA.

    If the output section has an explicit VMA, the explicit address is honored.
    The region is used for memory usage accounting only when the explicit VMA is
    within the region bounds.

<lma-region>
    Specifies the LMA placement memory region (optional) using ``AT>REGION``.

    ELD places the section's LMA at the next available address in the selected
    region, tracked independently from the VMA region cursor. ``AT(<address>)``
    and ``AT>REGION`` are mutually exclusive.

<fillexp>
    Specifies the fill value of the output section (optional). The value can be an expression. This option is parsed, but it has no effect on linking.

<phdr>
    Specifies a program segment for the output section (optional). To assign multiple program segments to an output section, this option can appear more than once in an output section description.

INSERT AFTER <section-name> | INSERT BEFORE <section-name>
    Requests placing this output section after/before the named output section.
    The anchor section must exist, or the linker emits an error. Overlay member
    sections do not support output section epilogues, so INSERT is not allowed
    inside OVERLAY member blocks.

Sorting input sections
----------------------

Linker scripts can control input-section ordering within an output section using
sorting directives in input section descriptions. These directives include
``SORT``, ``SORT_BY_NAME``, ``SORT_BY_ALIGNMENT``, and
``SORT_BY_INIT_PRIORITY``.

ELD also supports the GNU linker shorthand ``SORT(CONSTRUCTORS)`` for
compatibility with other linkers.

.. _controlling-physical-addresses:

Controlling Physical addresses
-------------------------------

In GNU linker scripts, the AT command is used to control the Load Memory Address (LMA) of a section, while the section's placement in memory during execution is defined by its Virtual Memory Address (VMA).


.. important::

   When an AT command is specified as part of the output section, the linker
   will not automatically align the load memory address of the section.

ALIGN_WITH_INPUT attribute on an output section preserves the VMA-to-LMA offset
from the previous output section when both sections use the same VMA region and
the same LMA region. If either region changes, the linker does not reuse the
prior offset and instead computes the LMA from the current output section's
placement rules. This matches GNU behavior when combining explicit LMA control
with region-based placement.

Behavior summary:

- VMA placement is governed by the output section's virtual address rules and
  the selected VMA region.
- LMA placement is governed by the AT/AT> directives and the selected LMA
  region, and it is tracked independently from the VMA placement.
- ALIGN_WITH_INPUT preserves the prior VMA-to-LMA delta, but only while both
  regions remain the same.

See also:

- :file:`test/Common/standalone/linkerscript/AlignWithInput/NoPhdrs/AlignWithInput.test`
- :file:`test/Common/standalone/linkerscript/AlignWithInput/TLS/TLS.test`
- :file:`test/Common/standalone/linkerscript/AlignWithInput/NoLoadATRAM/NoLoadATRAM.test`

GNU-compatibility
--------------------

The eld linker script syntax and semantics are GNU-compliant. This means that
any linker script that works with the GNU linker should also work with eld,
with the exception of a few GNU linker script features that are not yet
supported by eld.

Previously, eld supported two extensions to the GNU linker script syntax.
**These extensions are no longer supported.** Any scripts using these
extensions must be updated to maintain compatibility with eld.
These extensions are:

1) Assignment without space between the symbol and :code:`=`

Previously supported::

    symbol=<expr>

GNU-compliant syntax (required now)::

    symbol = <expr>

GNU requires a space between the symbol and the assignment operator.
eld now enforces this requirement. Scripts must be updated accordingly.

2) Output section description without space between the output section name and :code:`:`

Previously supported::

    SECTIONS {
      FOO: {
        *(.text.foo)
      }
    }

GNU-compliant syntax (required now)::

    SECTIONS {
      FOO : {
        *(.text.foo)
      }
    }

GNU requires a space between the output section name and the colon.
eld now enforces this requirement for full GNU compatibility.

Why cannot eld support these extensions along with GNU-compatibility?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

eld cannot support these extensions along with GNU-compatibility because they
directly conflict with the GNU linker script syntax. For example, GNU ld
allows :code:`:` in section names and allows :code:`=` in symbol names. The
core issue is that GNU ld uses the same lexing state to parse symbol and
section names to keep the parser simple and efficient. Due to this, GNU ld
also allows other non-trivial characters in symbol names such as :code:`+`,
:code:`-`, :code:`:` and so on. For example, for the below linker script
snippet, gnu ld creates a symbol of the name :code:`a+=`::

  a+= = b # lhs symbol is a+=

eld cannot easily add exception to the two cases that were supported by eld extensions
while keeping everything else the same to keep the linker script parser efficient.
To support these as an exception, the parser needs to lookahead two tokens to resolve
ambiguities. Let's understand this with the help of an example::

  SECTIONS {
    FOO: {
      *(.text.foo)
    }
    u=v;
  }

When parsing the :code:`SECTIONS` commands, the parser does not know in which
LexState to parse the command. If the command is an output section description,
:code:`FOO:`, then the parser should parse the token in :code:`LexState::default`,
whereas if the command is an assignment, then the parser should parse the token in
:code:`LexState::Expr`. :code:`LexState::default` allows some characters
in tokens that are not appropriate when parsing an expression. These characters
include :code:`+`, :code:`-`, :code:`=` and more.

To correctly determine which :code:`LexState` to use, the parser needs to
peek (lookahead) two tokens in :code:`LexState::Expr`. With the two tokens peek,
the parser can determine whether the command is an assignment command or not.

This simple change requires a lot of changes in the parser. The parser needs to
change from LL(1) (Simple and efficient) to LL(2) (Complex and less efficient).


PRINT Command
--------------

.. _linker-script-print:

.. contents::
   :local:

Overview
^^^^^^^^^

The ``PRINT`` command lets a linker script emit formatted messages on the
linker's standard output stream while the script is being parsed and
evaluated. This is useful for debugging script expressions, inspecting
symbol values, or annotating map files during development.

Syntax
^^^^^^^

The ``PRINT`` command has the following syntax::

  PRINT("format-string", expression, ...)

Where:

- ``format-string`` is a C ``printf``-style format string.
- Each ``expression`` is a regular linker script expression whose value is
  substituted into the format string.

The command does not define any symbols and does not affect the layout or
contents of the linked image; it only produces textual output.

Format String Semantics
^^^^^^^^^^^^^^^^^^^^^^^^

Basics
^^^^^^^^

The format string closely follows C ``printf`` semantics with a limited set
of conversion specifiers:

- ``%%``  – prints a literal ``%`` character; consumes no argument.
- ``%d``  – signed decimal integer.
- ``%i``  – signed decimal integer (alias for ``%d``).
- ``%u``  – unsigned decimal integer.
- ``%o``  – unsigned octal integer.
- ``%x``  – unsigned hexadecimal integer (lowercase digits).
- ``%X``  – unsigned hexadecimal integer (uppercase digits).
- ``%c``  – single character; the low 8 bits of the numeric expression.
- ``%s``  – symbol name corresponding to a *symbol expression*.

All numeric expressions are evaluated as 64-bit integers. Length modifiers
are accepted for compatibility but ignored; the value is always normalized
to 64 bits before formatting.

Flags, Width, and Precision
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following flag characters are recognized:

- ``-`` – left-justify within the field width.
- ``+`` – always include a sign for signed conversions.
- `` `` (space) – prefix a space for positive signed conversions.
- ``#`` – alternate form (for example, add ``0x`` for hex).
- ``0`` – pad numeric fields with leading zeros.

Field width and precision have the usual ``printf`` syntax::

  %[flags][width][.precision][length]conversion

with the following constraints:

- ``width`` and ``precision`` must be decimal integer constants.
- ``*`` is **not** supported for either width or precision; using it
  produces a diagnostics error.
- Supported length modifiers (``hh``, ``h``, ``l``, ``ll``, ``j``, ``z``,
  ``t``, ``L``) are parsed but ignored – values are always treated as
  64-bit.

Escape Sequences
^^^^^^^^^^^^^^^^^

The format string supports a small set of C-style escape sequences, which
are unescaped before formatting:

- ``\\n`` – newline
- ``\\t`` – horizontal tab
- ``\\r`` – carriage return
- ``\\\\`` – backslash
- ``\\\"`` – double quote

Any other escape sequence is left unchanged (the leading backslash is kept
as-is).

Argument Matching and Errors
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The linker validates the correspondence between conversion specifiers in
the format string and the expressions supplied to ``PRINT``. The following
conditions are diagnosed as ``PRINT`` errors:

- Not enough arguments:

  - The format string requires more values than the number of expressions
    supplied (for example, ``PRINT("%d %u", 1);``).

- Too many arguments:

  - More expressions are supplied than conversion specifiers in the format
    string (for example, ``PRINT("%d", 1, 2);``).

- Unterminated or malformed format specifier:

  - A ``%`` at the end of the string (for example, ``"value %"``).
  - A partially specified format that never reaches a conversion character.

- Unsupported conversion:

  - Any conversion character other than ``d``, ``i``, ``u``, ``o``, ``x``,
    ``X``, ``c``, ``s``, or ``%`` (for example, ``%f``) is rejected.

- Unsupported width or precision:

  - Using ``*`` for width (for example, ``"%*d"``).
  - Using ``*`` for precision (for example, ``"%. *d"``).

- Invalid ``%s`` argument:

  - The expression corresponding to a ``%s`` conversion must be a *symbol
    expression* (such as ``foo``). Passing an arbitrary numeric expression
    (for example, ``1 + 2``) is rejected.

On any of these conditions the linker emits an ``error_printcmd`` diagnostic
and treats the ``PRINT`` invocation as a fatal error for that link.

Examples
^^^^^^^^^^

Printing Numeric Expressions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block::

  /* Signed and unsigned forms */
  PRINT("value=%d (0x%x)\\n", 42, 42);

  SECTIONS {
    .text : { *(.text*) }
  }

This prints a line similar to::

  value=42 (0x2a)

Using ``%c`` and ``%s``
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block::

  /* Assume 'foo' is a symbol defined by an input object. */
  PRINT("symbol %s at '%c' section start\\n", foo, 'T');

The ``%s`` conversion prints the symbol name (``foo``) while ``%c`` prints
the low 8 bits of the numeric expression.

Using Width, Precision, and Flags
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block::

  PRINT("val=%+08d hex=%#06x\n", 42, 42);

  SECTIONS {
    .text : { *(.text*) }
  }

This uses:

- ``+`` to always show the sign for the decimal value.
- ``0`` and a width of ``8`` to zero-pad the decimal field (for example,
  ``+0000042``).
- ``#`` and a width of ``6`` for hexadecimal, causing a leading ``0x`` and
  zero-padding in the remaining field (for example, ``0x002a``).

Error Examples
^^^^^^^^^^^^^^^

.. code-block::

  /* Not enough arguments */
  PRINT("x=%d y=%d\\n", 1);

  /* Unsupported conversion */
  PRINT("value=%f\\n", 1);

  /* Invalid %s argument */
  PRINT("value=%s\\n", 1 + 2);

Each of these invocations produces a ``PRINT``-related diagnostic and
prevents the link from succeeding.
