====================
Image Layout
====================

At its core, the linker is responsible for transforming compiled object files into a final executable image. This process includes resolving symbols, relocating sections, and most importantly, laying out the image in memory.

Image layout refers to how the linker arranges code and data in memory when producing an executable or binary. There are two main approaches:

* Using default behavior
* Using custom linker scripts

Before we start defining how the linker does image layout, it would be useful to
understand the distinction between some of the terminology used in the linker.

Input Sections
----------------

These come from object files (.o) and libraries (.a) compiled from source code.

Examples:

* .text — code
* .data — initialized data
* .bss — uninitialized data
* .rodata — read-only data

Output sections
----------------
These are the merged and organized sections in the final binary, created by the linker.

The linker collects input sections of the same type and merges them into output sections.

You can rely on the default rules and built-in heuristics provided by the
linker.

You can also control this mapping by using a linker script.

Segments
---------
Segments are defined in the ELF program header and describe how the binary should be loaded into memory at runtime.

Key Points:

* Segments are containers for output sections
* They define memory permissions (read, write, execute)
* Used by the runtime loader

There are two types of segments

* non-loadable segments
* loadable segments

Segment alignment
------------------

Empty segments
~~~~~~~~~~~~~~~

* Loadable segments all have segment align set to the page size set at link time
* Non loadable segments have the segment alignment set to the minimum word size of the output ELF file

Non Empty segments
~~~~~~~~~~~~~~~~~~~

* Loadable non empty segments have the segment alignment set to the page size
* Non loadable segments are set to the maximum section alignment of the containing sections in the segment

.. note::

  omagic (linker command line option --omagic/-N)

  With omagic, segment alignment is set to the maximum section alignment of the containing sections in the segment


Using default behavior
-----------------------
When a linker performs layout without an explicit linker script, it relies on default rules and built-in heuristics provided by the linker implementation.

The linker has a default memory layout that defines:

* The order of sections (e.g., .text, .data, .bss)
* Default starting addresses (e.g., code starts at 0x08000000 for embedded systems or 0x400000 for ELF binaries on Linux)
* Alignment requirements for each section
* Default page alignment
* Whether program headers are loaded or not loaded

Using custom linker scripts
---------------------------

.. note::

   When using eld with a custom linker script, all default assumptions and behaviors built into the linker are overridden. The script provides complete control over the memory layout and section placement, effectively replacing the linker's internal defaults.

Linker scripts define how input sections (like .text, .data, .bss) are mapped to output sections and where they are placed in memory. These scripts control:

* Memory regions (e.g., RAM, ROM)
* Section alignment
* Ordering of code and data
* Load vs. execution addresses

This is the standard method used by most linkers like GNU ld.

PHDRS
~~~~~~

The PHDRS (Program Headers) command in a linker script is used to define program headers in the output binary, particularly for ELF (Executable and Linkable Format) files. These headers are essential for the runtime loader to understand how to load and map the binary into memory.

When and Why PHDRS is Used
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Custom Memory Mapping
""""""""""""""""""""""""

You use PHDRS when you want to explicitly control how sections are grouped into segments in the ELF file. This is especially important for:

- Embedded systems
- Custom bootloaders
- OS kernels

Fine-Grained Segment Control
"""""""""""""""""""""""""""""

- Assign specific sections to specific segments
- Control segment flags (e.g., PT_LOAD, PT_NOTE, PT_TLS)
- Set permissions (r, w, x) for each segment

Avoiding Default Segment Layout
"""""""""""""""""""""""""""""""""

Without PHDRS, the linker automatically generates segments based on section types. If you need non-standard layout, PHDRS is required.


Advanced Control via Linker Plugins (ELD Linker)
-------------------------------------------------
Particularly with the ELD linker, image layout is further enhanced using linker plugins. These plugins provide finer-grained control than traditional scripts.

* Custom Layout Logic: Plugins can override default section placement logic to optimize for cache locality or hardware-specific constraints.
* Dynamic Behavior: Unlike static scripts, plugins can make layout decisions based on runtime metadata or build-time heuristics.
* Programmatic Layout: Developers can write plugins (e.g., LayoutOptimizer) that programmatically define how sections are arranged, enabling more flexible and performance-tuned binaries

This approach is especially useful in embedded systems where:

* Memory is constrained and fragmented.
* Performance depends on precise placement of code/data.
* Debugging requires reproducible and traceable layouts.

How the linker does image layout ?
------------------------------------
