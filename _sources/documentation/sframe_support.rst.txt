SFrame Support
==============

.. contents::
   :local:

Overview
--------

SFrame (Simple Frame) is a lightweight debugging format that provides stack unwinding information for debuggers and profilers. ELD provides comprehensive support for processing `.sframe` sections and creating SFrame headers in linked executables and shared libraries.

The SFrame format is designed to be much simpler and more compact than DWARF debug information, focusing solely on the minimal information needed for stack unwinding:

* **Canonical Frame Address (CFA)**: The address of the call frame
* **Frame Pointer (FP)**: Location of the frame pointer
* **Return Address (RA)**: Location of the return address

ELD implements support for **SFrame version 2 (errata 1)** as specified in the SFrame Format documentation.

SFrame vs EhFrame
-----------------

SFrame offers several advantages over the traditional EhFrame format:

**Size Efficiency**
   SFrame sections are significantly smaller than equivalent EhFrame sections, resulting in reduced binary size and memory usage.

**Parsing Speed**
   The simplified format allows for faster parsing during stack unwinding operations.

**Reduced Complexity**
   SFrame eliminates much of the complexity of DWARF-based unwinding while maintaining the essential functionality.

**Better Cache Performance**
   The compact format leads to better cache utilization during runtime stack unwinding.

Command Line Options
--------------------

ELD provides the following command line option for SFrame support:

``--sframe-hdr``
   Create an SFrame header section and process `.sframe` sections in input files. This option enables:

   * Processing of `.sframe` sections from input object files
   * Creation of consolidated SFrame sections in the output
   * Proper linking and address resolution for SFrame data
   * Integration with ELD's section layout and memory management

Usage Examples
--------------

Basic Linking with SFrame
~~~~~~~~~~~~~~~~~~~~~~~~~~

To link object files containing `.sframe` sections and create an SFrame header:

.. code-block:: bash

   # Create source file with SFrame section
   cat > source1.s << 'EOF'
   .text
   .globl func1
   func1:
       ret
   .section .sframe,"a"              # Type automatically set to SHT_GNU_SFRAME (llvm-mc 22.x+)
   .byte 0xe2, 0xde, 0x02, 0x08      # SFrame header with magic and version
   EOF

   # Assemble files containing SFrame sections (requires llvm-mc 22.x or newer)
   llvm-mc -filetype=obj -triple=x86_64-linux-gnu source1.s -o source1.o
   llvm-mc -filetype=obj -triple=x86_64-linux-gnu source2.s -o source2.o

   # Link with SFrame header creation
   eld --sframe-hdr source1.o source2.o -o executable

.. note::
   For llvm-mc versions prior to 22.x, the section type must be specified explicitly:

   .. code-block:: assembly

      .section .sframe,"a",@0x6ffffff4  # Explicit SHT_GNU_SFRAME type for older assemblers

Shared Library with SFrame
~~~~~~~~~~~~~~~~~~~~~~~~~~~

SFrame sections can also be included in shared libraries:

.. code-block:: bash

   # Create shared library with SFrame information
   eld --sframe-hdr -shared libfoo.o -o libfoo.so

Combining with Other Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

SFrame support works seamlessly with other ELD features:

.. code-block:: bash

   # SFrame with garbage collection
   eld --sframe-hdr --gc-sections input.o -o output

   # SFrame with optimization
   eld --sframe-hdr -O2 input.o -o output

   # SFrame with debug symbols
   eld --sframe-hdr --debug input.o -o output

Section Processing
------------------

Input Processing
~~~~~~~~~~~~~~~~

ELD processes `.sframe` sections from input object files by:

1. **Recognition**: Identifying `.sframe` sections in ELF input files
2. **Parsing**: Validating SFrame headers and parsing FRE (Frame Row Entry) data
3. **Merging**: Combining multiple `.sframe` sections from different input files
4. **Address Resolution**: Resolving function addresses and updating SFrame data

Output Generation
~~~~~~~~~~~~~~~~~

When the `--sframe-hdr` option is specified, ELD creates:

* **Consolidated .sframe section**: Contains all SFrame data from input files
* **Proper section alignment**: Ensures correct alignment for runtime access
* **Address fixups**: Updates all address references to final linked addresses
* **Section headers**: Creates appropriate ELF section headers for SFrame data

Supported Architectures
-----------------------

ELD's SFrame support is available for the architectures with ABI identifiers
defined in the `SFrame specification <https://sourceware.org/binutils/docs/sframe-spec.html>`_:

* **AArch64**: ARM 64-bit architecture (little-endian and big-endian)
* **AMD64 (x86_64)**: AMD/Intel 64-bit architecture (little-endian)

Other architectures are not yet assigned ABI identifiers in the SFrame
specification and are not currently supported.

Integration with Debugging Tools
---------------------------------

SFrame sections created by ELD are compatible with tools that support the
SFrame format:

* **GDB**: GNU Debugger (version 16+) can use SFrame information for stack unwinding
* **libsframe**: The libsframe library (from GNU binutils) can parse SFrame sections

Garbage Collection Behavior
----------------------------

When using ``--gc-sections`` with SFrame support:

* **Preservation**: ``.sframe`` sections are not subject to garbage collection and are preserved in the output

Error Handling
--------------

ELD provides comprehensive error detection for SFrame processing:

**Invalid Magic Number**
   Reports errors for SFrame sections with incorrect magic numbers (expected: 0xdee2)

**Unsupported Version**
   Detects and reports unsupported SFrame versions (ELD supports version 2)

**Truncated Sections**
   Identifies and reports SFrame sections with incomplete data

**Address Resolution Failures**
   Reports errors when SFrame function addresses cannot be resolved

Example error output:

.. code-block:: text

   Error: SFrame Read Error : invalid SFrame magic number from file input.o
   Error: SFrame Read Error : unsupported SFrame version from file input.o
   Error: SFrame Read Error : section too small for SFrame header from file input.o

Performance Considerations
--------------------------

SFrame support in ELD is designed for optimal performance:

**Link Time**
   * Minimal overhead during linking
   * Efficient parsing of SFrame sections
   * Fast address resolution and fixups

**Runtime**
   * Compact sections reduce memory usage
   * Fast parsing during stack unwinding
   * Improved cache locality

**Binary Size**
   * Significantly smaller than equivalent DWARF information
   * Optional inclusion based on build requirements

Best Practices
--------------

1. **Consistent Usage**: Use `--sframe-hdr` consistently across all objects in a project
2. **Testing**: Verify SFrame functionality with debugging tools after linking
3. **Architecture Specific**: Ensure SFrame generation tools target the correct architecture
4. **Integration**: Combine with appropriate optimization levels for best results
5. **Validation**: Use readelf or similar tools to verify SFrame section contents

Troubleshooting
---------------

**Missing SFrame Sections**
   Ensure input objects were compiled with SFrame generation enabled

**Incorrect Function Addresses**
   Verify that all input objects use consistent compilation flags

**Debugger Issues**
   Check that debugging tools support SFrame format version 2

**Performance Problems**
   Consider the trade-off between SFrame inclusion and binary size requirements

For additional support and troubleshooting, consult the ELD diagnostic output when using the `--verbose` option along with `--sframe-hdr`.
