Linker Script
===============
.. contents::
   :local:

Overview
------------
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

Basic script syntax
--------------------

 - Symbols

   Symbol names must begin with a letter, underscore, or period. They can include letters, numbers, underscores, hyphens, or periods.

 - Comments

   Comments can appear in linker scripts

 - Strings

   Character strings can be specified as parameters with or without delimiter characters.

 - Expressions

   Expressions are similar to C, and support all C arithmetic operators. They are evaluated as type long or unsigned long

 - Location Counter

   | A period is used as a symbol to indicate the current location counter. It is used in the SECTIONS command only, where it designates locations in the output section:
   | . = ALIGN(0x1000); . = . + 0x1000;
   | Assigning a value to the location counter symbol changes the location counter to the specified value.
   | The location counter can be moved forward by arbitrary amounts to create gaps in an output section.
   | It cannot, however, be moved backwards.

 - Symbol assignment

   | Symbols, including the location counter, can be assigned constants or expressions:
   | __text_start = . + 0x1000;
   | Assignment statements are similar to C, and support all C assignment operators. Terminate assignment statements with a semicolon

Script commands
----------------

    The SECTIONS command must be specified in a linker script. All the other script commands are optional.

     - PHDRS

        Syntax :- { name type [FILEHDR][PHDRS][AT (address)][FLAGS (flags)] }

        The PHDRS script command sets information in the program headers, also known as *segment header* of an ELF output file.

            * name – Specifies the program header in the SECTIONS command
            * type – Specifies the program header type
            * PT_LOAD – Loadable segment
            * PT_NULL – Linker does not include section in a segment. No loadable section should be set to PT_NULL.
            * PT_DYNAMIC – Segment where dynamic linking information is stored
            * PT_INTERP – Segment where the name of the dynamic linker is stored
            * PT_NOTE – Segment where note information is stored
            * PT_SHLIB – Reserved program header type
            * PT_PHDR – Segment where program headers are stored
            * FLAGS - Specifies the p_flags field in the program header
               * The value of flags must be an integer. It is used to set the p_flags field of the program header: for instance, FLAGS(5) sets p_flags to PF_R | PF_X; and FLAGS(0x03000000) sets OS-specific flags.

            .. note::
               If the sections in an output file have different flag settings than what is specified in PHDRS, the linker combines the two different flags using bitwise or

     - SECTIONS

        Syntax :- SECTIONS { section_statement section_statement ... }

        The SECTIONS script command specifies how input sections are mapped to output sections, and where output sections are located in memory. The SECTIONS command must be specified once, and only once, in a linker script.

        - Section statements

            A SECTIONS command contains one or more section statements, each of which can be one of the following:

               * An ENTRY command
               * A symbol assignment statement to set the location counter. The location counter specifies the default address in subsequent section-mapping statements that do not explicitly specify an address.
               * An output section description to specify one or more input sections in one or more library files, and maps those sections to an output section. The virtual memory address of the output section can be specified using attribute keywords.

     - ENTRY

        Syntax :- ENTRY (symbol)

        * The ENTRY script command specifies the program execution entry point.

        * The entry point is the first instruction that is executed after a program is loaded.

        * This command is equivalent to the linker command-line option,-e.

     - OUTPUT_FORMAT

        Syntax :- OUTPUT_FORMAT (string)

        * The OUTPUT_FORMAT script command specifies the output file properties.

        * For compatibility with the GNU linker, this command is parsed but has no effect on linking.

     - OUTPUT_ARCH

        Syntax :- OUTPUT_ARCH ("aarch64")

        * The OUTPUT_ARCH script command specifies the target processor architecture.

        * For compatibility with the GNU linker, this command is parsed but has no effect on linking.

     - SEARCH_DIR

        Syntax :- SEARCH_DIR (path)

        * The SEARCH_DIR script command specifies which adds the specified path to the list of paths that the linker uses to search for libraries.

        * This command is equivalent to the linker command-line option,-L.

     - INCLUDE

        Syntax :- INCLUDE (file)

        * The INCLUDE script command specifies the contents of the text file at the current location in the linker script.

        * The specified file is searched for in the current directory and any directory that the linker uses to search for libraries.

        .. note:: Include files can be nested.

     - OUTPUT

        Syntax :- OUTPUT (file)

        * The OUTPUT script command defines the location and file where the linker will write output data.

        * Only one output is allowed per linking.

     - GROUP

        Syntax :- GROUP (file, file, …)

        * The GROUP script command includes a list of achieved file names.

        * The achieved names defined in the list are searched repeatedly until all defined references are resolved.

     - ASSERT

        Syntax :- ASSERT(expression, string)

        * The ASSERT script command adds an assertion to the linker script.


Expressions
------------

    Expressions in linker scripts are identical to C expressions

     +--------------------------------------+------------------------------------------------------------------------------------------+
     | Function                             |  Description                                                                             |
     +======================================+==========================================================================================+
     |  .                                   | Return the location counter value representing the current virtual address.              |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | ABSOLUTE (expression)                | Return the absolute value of the expression.                                             |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | ADDR (string)                        | Return the virtual address of the symbol or section. Dot (.) is supported.               |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | ALIGN (expression)                   | Return value when the current location counter is aligned to the next expression         |
     |                                      | boundary. The value of the current location counter is not changed.                      |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | ALIGN (expression1, expression2)     | Return value when the value of expression1 is aligned to the next expression2 boundary.  |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | ALIGNOF (string)                     | Return the align information of the symbol or section.                                   |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | ASSERT (expression, string)          |  Throw an assertion if the expression result is zero.                                    |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | BLOCK (expression)                   | Synonym for ALIGN (expression).                                                          |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | DATA_SEGMENT_ALIGN(maxpagesize       |   Equivalent to:                                                                         |
     |   , commonpagesize)                  |   (ALIGN(maxpagesize)+(.&(maxpagesize ‑ 1))) or                                          |
     |                                      |   (ALIGN(maxpagesize)+(.&(maxpagesize -commonpagesize)))                                 |
     |                                      |   The linker computes both of these values and returns the larger one.                   |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | DATA_SEGMENT_END (expression)        | Not used; return the value of the expression.                                            |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | DATA_SEGMENT_RELRO_END               | Not used; return the value of the expression.                                            |
     | (expression )                        |                                                                                          |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | DEFINED (symbol)                     | Return 1 if the symbol is defined in the global symbol table of the linker.              |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | LOADADDR (string)                    | Synonym for ADDR (string).                                                               |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | MAX (expression, expression)         | Return the maximum value of two expressions.                                             |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | MIN (expression1, expression2)       | Return the minimum value of two expressions.                                             |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | SEGMENT_START (string, expression)   | If a string matches a known segment, return the start address of                         |
     |                                      |   that segment. If nothing is found, return the value of the expression.                 |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | SIZEOF (string)                      |   Return the size of the symbol, section, or segment.                                    |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | SIZEOF_HEADERS                       | Return the section start file offset.                                                    |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | CONSTANT (MAXPAGESIZE)               | Return the defined default page size required by ABI.                                    |
     +--------------------------------------+------------------------------------------------------------------------------------------+
     | CONSTANT (COMMONPAGESIZE)            | Return the defined common page size.                                                     |
     +--------------------------------------+------------------------------------------------------------------------------------------+

Symbol assignment
--------------------

    * Any symbol defined in a linker script becomes a global symbol. The following C assignment operators
      are supported to assign a value to a symbol:

    * symbol=expression;
    * symbol+=expression;
    * symbol-=expression;
    * symbol*=expression;
    * symbol/=expression;
    * symbol&=expression;
    * symbol|=expression;
    * symbol<<=expression;
    * symbol>>=expression;

    ..note:: The first statement above defines symbol and assigns it the value of expression. In
    the other statements, symbol must already be defined

    * All the statements above must be terminated with a semicolon character.
    * One way to create an empty space in memory is to use the expression.+=space_size: BSS1 { . += 0x2000 }
    * This statement generates a section named BSS1 with size 0x2000

    +--------------------------------------+------------------------------------------------------------------------------------------+
    | Function                             |  Description                                                                             |
    +======================================+==========================================================================================+
    | HIDDEN (symbol = expression)         | Hide the defined symbol so it is not exported.                                           |
    +--------------------------------------+------------------------------------------------------------------------------------------+
    | FILL (expression)                    | Specify the fill value for the current section. The fill length can be                   |
    |                                      |  1, 2, 4, or 8. The linker determines the length by selecting the                        |
    |                                      |  minimum fit length. In the following example, the fill length is 8:                     |
    |                                      |                                                                                          |
    |                                      | FILL( 0xdeadc0de )                                                                       |
    |                                      | A FILL statement covers memory locations from the point at                               |
    |                                      | which it occurs to the end of the current section.                                       |
    |                                      | Multiple FILL statements can be used in an output section                                |
    |                                      | definition to fill different parts of the section with different patterns.               |
    +--------------------------------------+------------------------------------------------------------------------------------------+
    | ASSERT (expression, string)          | When the specified expression is zero, the linker throws an                              |
    |                                      | assertion with the specified message string.                                             |
    +--------------------------------------+------------------------------------------------------------------------------------------+
    | PROVIDE (symbol = expression)        | Similar to symbol assignment, but does not perform checking for  an unresolved reference |
    +--------------------------------------+------------------------------------------------------------------------------------------+
    | PROVIDE_HIDDEN (symbol = expression) | Similar to PROVIDE, but hides the defined symbol so it will not be exported.             |
    +--------------------------------------+------------------------------------------------------------------------------------------+
    | PRINT (symbol = expression)          | Instruct the linker to print symbol name and expression value to                         |
    |                                      | standard output during parsing                                                           |
    +--------------------------------------+------------------------------------------------------------------------------------------+

NOCROSSREFS
---------------
     * The NOCROSSREFS command takes a list of space-separated output section names as its arguments.

     * Any cross references among these output sections will result in link editor failure.

     * The list can also contain an orphan section that is not specified in the linker script.

     * A linker script can contain multiple NOCROSSREFS commands.

     * Each command is treated as an independent set of output sections that are checked for cross references.

Output Section Description
==========================

A ``SECTIONS`` command can contain one or more output section descriptions.

.. code-block:: plaintext

    <section-name> [<virtual_addr>][(<type>)] :
    [AT(<load_addr>)] [ALIGN(<section_align>) | ALIGN_WITH_INPUT]
    [SUBALIGN(<subsection_align>)] [<constraint>]
    {
       ...
       <output-section-command> <output-section-command>
    }[><region>][AT><lma_region>][:<phdr>...][
    =<fillexp>]

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
    Specifies the region of the output section (optional). The region is expressed as a string. This option is parsed but has no effect on linking.

<lma-region>
    Specifies the load memory address (LMA) region of the output section (optional). The value can be an expression. This option is parsed, but it has no effect on linking.

<fillexp>
    Specifies the fill value of the output section (optional). The value can be an expression. This option is parsed, but it has no effect on linking.

<phdr>
    Specifies a program segment for the output section (optional). To assign multiple program segments to an output section, this option can appear more than once in an output section description.

.. note::

    ALIGN_WITH_INPUT currently does not do anything in the linker, linker defaults to always align the physical address according to the requirements of the output section
