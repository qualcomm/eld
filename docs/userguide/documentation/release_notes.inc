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

* **Multi-Architecture Support**: SFrame functionality available across all supported architectures including AArch64, x86_64, ARM, RISC-V, Hexagon, and MIPS.

* **Performance Optimized**: SFrame sections provide significantly reduced size compared to DWARF debug information while maintaining essential stack unwinding capabilities.

* **Debugger Integration**: Generated SFrame sections are compatible with modern debugging tools including GDB, LLDB, and various profiling utilities.

* **Garbage Collection Aware**: SFrame sections are properly handled during ``--gc-sections`` operations, ensuring consistency between code and unwinding information.

The SFrame format offers substantial benefits for embedded and performance-critical applications where compact debugging information is essential. For detailed usage information, see the SFrame Support documentation.
