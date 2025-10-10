Image layout
##############

.. contents::
   :local:

At its core, the linker is responsible for transforming compiled object files into a
final image that is ready to run on the device. This process includes
resolving symbols, relocating sections, and most importantly, laying out the image in memory.

Image layout refers to the linker's assignment of addresses to code, data and metadata
determining their arrangement in memory when the program runs, and
to the placement of contents within the final output image.

In this document, we will take a deep dive into the linker's image layout process,
examining the factors that influence the layout and the reasoning behind the specific
choices the linker makes. Understanding why the image layout looks the way it does is challenging,
but it's invaluable for diagnosing and fixing subtle, hard-to-decode layout issues.

*But why is the image layout important?*

The image layout is critical for correctness, performance and security.
Among other things, it ensures alignment compliance, enables cache-friendly
placement of code and data, and enhances security by enforcing memory protection
policies such as preventing any executable page from having write permissions.
In embedded systems, image layout is especially critical due to tight memory constraints,
real-time performance needs, and hardware-specific placement requirements.

Before we dive deep into defining and understanding how the linker does image layout,
it would be useful to understand the distinction between some of the terminology used
in the linker.

Linker terminology
*******************

Input Sections
===============

A section holds content that can be code, data and metadata (Example: :code:`.text`
, :code:`.data`, :code:`.bss`, :code:`.comment`). The linker may reorder, merge
or discard whole sections but it treats each section as an indivisible unit.
The section header table describes each section's name, type, virtual address, file offset,
alignment and more. **The section header table and the sections are used by the linker during
linking.** These come from object files (.o) and libraries (.a) compiled from source code.

Examples:

* .text — code
* .data — initialized data
* .bss — uninitialized data
* .rodata — read-only data

Output sections
================

These are the merged and organized sections in the final binary, created by the linker.

The linker collects input sections of the same type and merges them into output sections.

You can rely on the default rules and built-in heuristics provided by the
linker.

You can also control this mapping by using a linker script.

Segments
=========

A segment is composed of one or more output sections and describes
how the program should be loaded into memory at runtime. In ELF, The program header table
describes each segment's type, virtual and physical address, file size, memory size, permissions,
and alignment requirements. **A loader only looks at the program header table to load an executable
into memory.** It does not care about the section header table.

Key Points:

* Segments are containers for output sections
* They define memory permissions (read, write, execute)
* Used by the runtime loader

Segment alignment
------------------

Empty segments
^^^^^^^^^^^^^^^^

* Loadable segments all have segment align set to the page size set at link time
* Non loadable segments have the segment alignment set to the minimum word size of
  the output ELF file

Non Empty segments
^^^^^^^^^^^^^^^^^^^^

* Loadable non empty segments have the segment alignment set to the page size
* Non loadable segments are set to the maximum section alignment of the containing
  sections in the segment


Now let's get started with understanding the image layout created by eld.

Image layout
**************

There are two main approaches of defining the image layout:

* Using default behavior
* Using custom linker scripts

Using default behavior
========================

When a linker performs layout without an explicit linker script, it relies on default rules and built-in heuristics provided by the linker implementation.

The linker has a default memory layout that defines:

* The order of sections (e.g., .text, .data, .bss)
* Default starting addresses (e.g., code starts at 0x08000000 for embedded systems or 0x400000 for ELF binaries on Linux)
* Alignment requirements for each section
* Default page alignment
* Whether program headers are loaded or not loaded

linker scripts
=================

.. note::

   When using eld with a custom linker script, all default assumptions and behaviors built into the linker are overridden. The script provides complete control over the memory layout and section placement, effectively replacing the linker's internal defaults.

The linker script primary job is to define the image layout requirements. It achieves
this with the :code:`SECTIONS`, :code:`PHDRS`, and the :code:`MEMORY` commands.

- :code:`SECTIONS` command specifies the output section properties and the mapping
  of input sections to the output sections.

- :code:`PHDRS` specifies which program headers (also known as segments) should be created
  by the linker. If :code:`PHDRS` is not specified, then the linker creates sensible PHDRs
  based on some default rules.

- :code:`MEMORY` command specifies the available memory regions. The output sections
  can then be assigned to particular memory regions. It provides a convenient way of
  arranging the output sections into memory.

:doc:`linker_script` describes these commands in more detail.

Here we will only highlight, or in some cases reiterate, some of the important and/or subtle
layout-related features and behavior.

Output section contents
-------------------------

An output section is composed of input sections. If no :code:`SECTIONS` command
is provided, the linker applies sensible default rules to match the input
sections to the output sections.

When a :code:`SECTIONS` command is present, the linker script input section description,
commonly referred to as linker script rule or just rule, control the input-output section mapping.
Input sections that match no rule are called orphans sections. Each orphan section is placed
into an output section with the same name; if no such output section exists, then the linker
creates a new one.

Image base address (starting address)
----------------------------------------

The default image base address depend upon the target, the linking type (whether the link
is creating an executable or a shared library), and whether the link contains a linker
script. The address 0 is used as the starting address if a linker script is present or
if the link is creating a shared library.

The default image base value can be overridden by using :option:`--image-base` command-line
option.

Output section virtual memory address (VMA)
---------------------------------------------

Multiple mechanisms can assign or influence the addresses of output sections.
A linker script can specify addresses explicitly; command-line options can set
section start addresses; and script directives such as :code:`MEMORY` allows
to conveniently arrange sections without hard-coding absolute addresses. Let's
examine all these methods, detail their behavior, and clarify their precedence.

Brief review of the methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1) No address/memory-region specified
"""""""""""""""""""""""""""""""""""""""

If no explicit address or MEMORY region is specified for an output section,
it is placed at the current value of the location counter (:code:`.``). The location counter
is initialized to the image base, and is automatically incremented whenever a content is added.
For example, if the value of the location counter is 0x1000 before assigning address to
:code:`.foo` output section (size 0x200), then :code:`foo` is placed at :code:`0x1000` and
the location counter advances to 0x1200.

The location counter can be explicitly incremented with a linker script assignment::

  . = . + ALIGN(0x8);

If an output section has an explicit address (or a memory region), then the location
counter is set to the address (or the valid address in the memory region) before placing
the output section.

There is an exception to this, orphan sections whose name ends with :code:`@<address>` are placed
exactly at the specified address.

2) Explicit linker script VMA address
"""""""""""""""""""""""""""""""""""""""

Linker script can specify an explicit address for an output section::

  section [address] :
  {
    output-section-command
    ...
  }

Let's see this in action with the help of an example:

.. container:: twocol

   .. container:: leftside

      .. code-block::

         // 1.c
         int foo() { return 1; }

         int bar() { return 3; }

   .. container:: rightside

      .. code-block::

         // script.t
         SECTIONS {
           .foo (0x1000) : { *(.text.foo) }
           .bar (0x2000) { *(.text.bar) }
         }

The linker assigns the exact addresses as specified in the linker script. :code:`.foo`
is assigned the VMA :code:`0x1000` and :code:`bar` is assigned the VMA :code:`0x2000`.

The linker assigns the exact addresses even if the addresses are not *exactly* aligned.
For the below example, :code:`.foo` will get VMA assigned to :code:`0x1001` and :code:`.bar`
will get VMA assigned to :code:`0x2002` even though they do not satisfy the alignment
requirements. In this case, the output section contains alignment padding at the beginning
such that the alignment requirements for the actual content (input sections) is satisfied.

.. container:: twocol

   .. container:: leftside

      .. code-block::

         // 1.c
         int foo() { return 1; }

         int bar() { return 3; }

   .. container:: rightside

      .. code-block::

         // script.t
         SECTIONS {
           .foo (0x1001) : { *(.text.foo) }
           .bar : AT(0x2002) { *(.text.bar) }
         }

However, if :code:`ALIGN` is also specified, then the explicit VMA is aligned to the
alignment specified in :code:`ALIGN`.

.. fixme:: Add example here!

3) Command-line options for setting section addresses
"""""""""""""""""""""""""""""""""""""""""""""""""""""""

There are various linker command-line options for setting output section
VMA: :option:`-Tbss`, :option:`-Tdata`, :option:`-Ttext` and
:option:`--section-start`.

When both the linker script and the command line specify an output-section address,
the command-line option takes precedence and overrides the script's explicit address.

4) Memory region
"""""""""""""""""""

Assigning an output section to a MEMORY region places it at the region's next available address,
subject to alignment and size constraints.

Let's understand this with the help of an example:

.. container:: twocol

   .. container:: leftside

      .. code-block::

         // 1.c
         int a;
         int u = 11;
         int v = 13;
         int foo() { return 1; }
         int bar() { return 3; }

  .. container:: rightside

      .. code-block::

         // script.t
         MEMORY {
           RAM : ORIGIN = 0x1000, LENGTH = 0x1000
         }

         SECTIONS {
           .data : { *(.data*) } >RAM
           .foo : { *(.text.foo) } >RAM
           .bss : {
             *(.bss*)
           } >RAM
           .bar : { *(.text.bar) } >RAM
         }

For the example above, the virtual addresses of the output section are shown below,
along with the assumed size and alignment.

- :code:`.data` (size: 0x8; alignment: 0x8): 0x1000
- :code:`.foo` (size: 0x8; alignment: 0x8): 0x1008
- :code:`.bss` (size: 0x4; alignment: 0x4): 0x1010
- :code:`.bar` (size: 0x8; alignment: 0x8): 0x1014

Precedence of virtual address assignment methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The precedence is:

1. Command-line options for setting section addresses
2. Explicit linker script VMA address
3. Memory region
4. No address/memory-region specified

Segment creation when :code:`PHDRS` is not specified
-------------------------------------------------------

eld uses a set of heuristics to decide when to start a new segment. Before detailing those heuristics,
we need to note how eld walks the layout:

eld processes output sections in the order specified by the linker script;
if no script is provided, then it uses the order in which the linker has
created default output sections.

We use the below terms to describe when eld creates a segment:

- Previous output section to refer to the output section that immediately precedes the current one
  in the traversal order.
- Previous allocatable output section to refer to the nearest preceding allocatable output section.
- Previous LOAD segment to refer to the segment of the nearest preceding allocatable output section.

With this traversal model and the terms in mind, we can now describe the segment-creation heuristics.

A new load segment is created when:

- There is no previous LOAD segment.

- Explicit output section address has been set using linker command-line options such as:
  :option:`-Ttext`, :option:`--section-start`, ...

- The segment flags required for the current output section is incompatible with the
  previous LOAD segment. By default, the segment flags :code:`R` and :code:`RE` are
  compatible, whereas :code:`RW` is incompatible with :code:`R` and :code:`RW`.

  :option:`--rosegment` and :option:`--omagic` influence which sections can be
  part of the same segment.

- The memory region of the current output section is different
  than the memory region of the previous allocatable output section.

- The previous output section type is NOBITS and the current output section
  type is PROGBITS.

- The previous output section virtual memory address is greater than
  the current output section virtual memory address.

- The VMA difference between the previous allocatable output section is greater
  than the segment alignment.

Other segment types:

- A :code:`PT_TLS` segment is created when an input file contains :code:`.tdata`/:code:`.tbss`.
- A :code:`PT_DYNAMIC` segment is created when a shared library or a dynamic executable is getting built.
- A :code:`PT_GNU_EH_FRAME` segment is created when the output contains :code:`eh_frame_hdr` section.


Segment creation when :code:`PHDRS` is specified
----------------------------------------------------

When the :code:`PHDRS` is specified in the linker script, then the linker only
creates the section specified in the :code:`PHDRS` command. No additional segments
are created.

Linker script assignment evaluation
-------------------------------------

Linker script assignment evaluation can influence the image layout as the location
counter value can be modified using a linker script assignment and the location counter
controls where the next content would be placed.

eld does not support lazy expression evaluation and forward references in expressions.

In eld, the linker script assignments outside the :code:`SECTIONS` command are evaluated
before the linker script assignments inside the :code:`SECTIONS` command. The linker script
assignments order is:

- First, all the linker script assignments outside the :code:`SECTIONS` command are evaluated
  in the specified order.
- Then all the linker script assignments inside the :code:`SECTIONS` command are evaluated
  in the specified order.

The linker script assignments specified using :option:`--defsym` are considered as
outside-:code:`SECTIONS` linker script assignments.

Linker options that affect layout
===================================

:option:`--rosegment`
----------------------

By default, readonly non-executable (:code:`R`) sections such as :code:`.rodata` sections and
executable sections (:code:`RX`) such as :code:`.text` can be part of the same segment. When
:option:`--rosegment` is specified, a different segment is created for readonly non-executable
segments.

:option:`--omagic`
-------------------

If :option:`--omagic` is specified, then readonly non-executable (:code:`R`),
executable (:code:`RX`), and read-write (:code:`RW`) sections can be part of the same segment.
Moreover, the segment alignment is set to the maximum section alignment instead of the page
alignment.

:option:`--align-segments`
----------------------------

This option cannot be used with linker scripts. When used,
the addresses of segments (both virtual and physical addresses)
are aligned to the page boundaries.

:option:`--enable-bss-mixing` and :option:`--disable-bss-conversion`
---------------------------------------------------------------------

These options control how the linker treats BSS sections relative to non-BSS
sections within the same segment.

:option:`--disable-bss-conversion` controls whether the linker converts BSS
to non-BSS when BSS/non-BSS sections are mixed. When this option is passed,
BSS remains NOBITS when BSS and Non-BSS are mixed in a segment. This option must
be combined with :option:`--enable-bss-mixing` if the BSS section will be placed before a non-BSS
section because otherwise BSS before non-BSS is an error.

This combination of options is useful for reducing file size of a program.
For example, if a program has a layout requirement that a BSS section
(let's say :code:`.bss`) must be placed before a non-BSS section (let's say :code:`.data`)
in the same segment, and the addresses of :code:`.bss` and :code:`.data` are :code:`0x1000`
and :code:`0x2000` respectively, then with the default behavior :code:`.bss` will be
converted to PROGBITS and 0s would need to be filled explicitly in the file for this section.
This can significantly increase the file size. On the contrary, when
:code:`--disable-bss-conversion --enable-bss-mixing`, :code:`.bss` will remain as NOBITS
and thus no file size will be consumed by this section.

Please note that with such a layout you may need a custom loader because most standard
loaders would not accept a layout where a BSS section is followed by a non-BSS section
in the same segment.

.. note::

   These options are currently supported only for ARM, AArch64, and RISCV targets.

Let's understand these options in more detail with the help of an example:

Below is a minimal example showing how these options affect layout when placing
both :code:`.bss` and :code:`.data` in the same segment.

bssmix.c::

  int bssvar;            // goes to .bss (NOBITS)
  int data = 1;          // goes to .data (PROGBITS)
  int main() { return data + bssvar; }

Linker script (script.t)::

  PHDRS {
    A PT_LOAD;
    B PT_LOAD;
  }

  SECTIONS {
    .text (0x1000) : { *(.text*) } :A
    .data (0x3000) : { *(.data*) } :B
    .bss  (0x2000) : { *(.bss*)  } :B
  }

Build and link::

  $ clang -o bssmix.o --target=aarch64-unknown-elf -c bssmix.c -ffunction-sections -fdata-sections

  # .bss section is promoted to PROGBITS because .bss is placed before .data in the segment B.
  $ ld.eld -o bssmix.out bssmix.o -T script.t

  # Error: Mixing BSS and non-BSS sections in segment. non-BSS '.data' is after
  # BSS '.bss' in linker script
  $ ld.eld -o bssmix.disable_conversion.out bssmix.o -T script.t --disable-bss-conversion

  # .bss section is placed before .data in the segment B and BSSness of .bss is preserved,
  # that is, it is not promoted to PROGBITS.
  $ ld.eld -o bssmix.disable_conversion.out bssmix.o -T script.t --disable-bss-conversion \
      --enable-bss-mixing

Advanced image layout control using linker plugins
====================================================

More finer-grained control over the image layout can be achieved using linker plugins.

* Custom Layout Logic: Plugins can override default section placement logic to optimize
  for cache locality or hardware-specific constraints.
* Dynamic Behavior: Unlike static scripts, plugins can make layout decisions based on
  runtime metadata or build-time heuristics.
* Programmatic Layout: Developers can write plugins (e.g., LayoutOptimizer) that programmatically
  define how sections are arranged, enabling more flexible and performance-tuned binaries

This approach is especially useful in embedded systems where:

* Memory is constrained and fragmented.
* Performance depends on precise placement of code/data.
* Debugging requires reproducible and traceable layouts.
