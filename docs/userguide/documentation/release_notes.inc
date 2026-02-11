===================================
Release notes for Version |release|
===================================

New Features
============

SFrame Support
--------------

ELD now provides comprehensive support for the SFrame (Simple Frame) debugging format:

* **SFrame Version 2 (Errata 1) Support**: Full implementation conforming to the latest SFrame specification for efficient stack unwinding information.

* **Command Line Option**: New ``--sframe-hdr`` option enables processing of ``.sframe`` sections and creation of SFrame headers in output binaries.

* **Architecture Support**: SFrame functionality available for architectures with ABI identifiers defined in the SFrame specification: AArch64 and AMD64 (x86_64).

* **Performance Optimized**: SFrame sections provide significantly reduced size compared to DWARF debug information while maintaining essential stack unwinding capabilities.

* **Debugger Integration**: Generated SFrame sections are compatible with tools supporting the SFrame format, including GDB 16+.

* **Garbage Collection Aware**: SFrame sections are properly handled during ``--gc-sections`` operations, ensuring consistency between code and unwinding information.

The SFrame format offers substantial benefits for embedded and performance-critical applications where compact debugging information is essential. For detailed usage information, see the SFrame Support documentation.
