Linker Map Files
#################

A map file provides detailed information about the link and helps analyze
output images, debug build issues, and better understand linker decisions.
It contains detailed information for input files, linker scripts, memory
regions, image memory layout, and so much more. Effectively using map files
can significantly reduce the time spent analyzing and debugging builds.

Use :option:`-Map` to generate map files for your builds.

.. contents::
   :local:

Map File Styles
****************

Map files are available in three distinct styles:

#. Text
#. YAML
#. Binary

Use option :option:`-MapStyle` to specify the map file style. Text map file
is the default map style.

Navigating Text Map File
*************************

Map file contents
==================

+---------------------------------------------+----------------------------------------------------------------+
| Section                                     |  Description                                                   |
+=============================================+================================================================+
| Tools Version, and System and Link features | Information about tools version, and system and link features. |
+---------------------------------------------+----------------------------------------------------------------+
| Link Stats                                  | Quick overview of key link information                         |
+---------------------------------------------+----------------------------------------------------------------+
| Archive Member Information                  | Specifies which archive members were included and why.         |
+---------------------------------------------+----------------------------------------------------------------+
| Allocating Common Symbols                   | Lists the common symbols which got allocated                   |
+---------------------------------------------+----------------------------------------------------------------+
| Linker Script and Memory Map                | Specifies input object files                                   |
+---------------------------------------------+----------------------------------------------------------------+
| Build statistics                            |                                                                |
+---------------------------------------------+----------------------------------------------------------------+
| Linker scripts used                         | Specifies the linker scripts used by the link.                 |
+---------------------------------------------+----------------------------------------------------------------+
| Linker plugin information                   | Details plugin information.                                    |
+---------------------------------------------+----------------------------------------------------------------+
| Output Section and Layout                   | Delineates output image layout                                 |
+---------------------------------------------+----------------------------------------------------------------+

Let's explore each of these sections in detail!

Tools Version, and System and Link Features
=================================================

As the title suggests, this consists information about tools version, and
system and link features. An example of this section is shown below::

   # Linker from QuIC LLVM Hexagon Clang version Version 19.0-alpha2 Engineering Release: hexagon-clang-190-229
   # Linker based on LLVM version: 19.0
   # Linker: d8a255719548f774e7e73cd97959ca04fd780b54
   # LLVM: 9ab18b8e7547380db4fb83b2ada1d7704876b334
   # Notable linker command/script options:
   # CPU Architecture Version:
   # Target triple environment for the link: unknown
   # Maximum GP size: 8
   # Link type: Static
   # ABI Page Size: 0x1000
   # CommandLine : hexagon-link -o main.elf main.o libs/libfoo.a libs/libbar.a -Map main.map.txt

Link Stats
===========

'Link Stats' provides a quick overview of key link information.
**Link stats are not displayed if their value is 0.**

An example of 'Link Stats' is shown below::

   # LinkStats Begin
   # ObjectFiles : 2
   # LinkerScripts : 2
   # ThreadCount : 64
   # NumInputSections : 37
   # ZeroSizedSections : 28
   # Total Symbols: 6 { Global: 3, Local: 2, Weak: 0, Absolute: 3, Hidden: 1, Protected: 0 }
   # Discarded Symbols: 1 { Global: 1, Local: 0, Weak: 0, Absolute: 0, Hidden: 1, Protected: 0 }
   # NumLinkerScriptRules : 2
   # NumOutputSections : 7
   # NumOrphans : 5
   # NoRuleMatches : 3
   # NumSegments : 1
   # OutputFileSize : 4920 bytes
   # LinkStats End

There might be more or less link stats than what is shown here depending on
your tools version. Remember that only non-zero stats are displayed.

Archive Member Information
===========================

'Archive Member Information' tells which archive members were included in the
link and due to which symbol. Archive members are only included in the link
if they satisfy an undefined symbol reference. The format of each entry of
'Archive Member Information' is::

   <ArchiveFile> (<ArchiveMember>)
        <CompilationUnitReferringArchiveMember> (<ReferencedSymbol>)

The below example shows that archive members 'libs/libfoo.a(Foo.o)' and
'libs/libbar.a(Bar.o)' are included in the link because 'main.o' referenced
symbols 'foo' and 'bar' from the archive members.

.. code-block:: bash

   Archive member included because of file (symbol)
   libs/libfoo.a(Foo.o)
                   main.o (foo)
   libs/libbar.a(Bar.o)
                   main.o (bar)

Allocating Common Symbols
==========================

* 'Allocating Common Symbols' list the common symbols that were allocated by the linker. Each entry in the common symbols section contains the following items:

    * The name of the symbol

    * The size of the memory area allocated for the symbol

    * The full pathname of the archive file accessed by the linker

    * The name of the archived object file that defines the symbol (in parentheses)

In the following example, the __eh_nodes symbol has size of **0x4** and is defined in the the object file: **xregister.o**

.. code-block:: bash
    :linenos:

    Common symbol	size	file

    __eh_nodes	0x4	$PATH/../target/hexagon/lib/v65/G0/libc.a(xregister.o)

Linker Script and Memory Map Section
=====================================

    * The linker script section of a link map lists the complete linker script that is specified for the link.

    .. note:: Linker scripts are optional. If a script is not specified on the linker command line, the link map does not include a linker script.

    * The following example shows the initial lines of a linker script section:


    .. code-block:: bash
        :linenos:

        Linker Script and memory map
        LOAD $PATH/../target/hexagon/lib/v65/G0/crt0_standalone.o[hexagonv65]
        LOAD target/main.o[hexagonv65]
        START GROUP
        LOAD $PATH/../target/hexagon/lib/v65/G0/libstandalone.a(_exit.o)[hexagonv65]
        ...
        LOAD $PATH/../target/hexagon/lib/v65/G0/libgcc.a(vmemcpy_h.o)[hexagonv65]
        END GROUP
        LOAD $PATH/../target/hexagon/lib/v65/G0/fini.o[hexagonv65]


Linker Scripts Used
=====================

'Linker Scripts Used' specifies the linker scripts used by the link.

    .. code-block:: bash
       :linenos:

       Linker scripts used (including INCLUDE command)
       target/linker.script


.. todo:: Add 'Linker Plugin Information' section docs!

.. todo:: Add 'Build statistics' section docs!

Output Section and Layout
==========================

'Output Section and Layout' describes the image layout.
It tells where output sections are placed and what they consist of. It presents
information in an elaborate format loosely based on the linker script
:code:`SECTIONS` command. This format allows users to easily visualize and
understand how :code:`SECTIONS` subcommands, linker script expressions and
assignment to the location counter affect the output layout.

'Output Section and Layout' contains detailed information
about output sections, input section descriptions (commonly called rules),
input sections, symbols, padding and linker script expressions. It is the most
complex part of the map file. Let's explore its structure to better understand
its content.

Simplified 'Output Section and Layout' Structure
-------------------------------------------------

We will first see its simplified structure. Padding and linker
script expressions are missing from this simplified structure. As we move
forward in the documentation, these missing components will be gradually
covered to make the structure complete.

<OutputSectionAndLayout>
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # Output Section and Layout
   <OutputSection_1>
   <OutputSection_2>
   ...
   <OutputSection_N>

Each <OutputSection> describes one output section. Let's see what it
consists of.

<OutputSection>
^^^^^^^^^^^^^^^^^

<OutputSection> is represented as:

.. code-block:: bash

   <OutputSectionName> <size> <VMA> # Offset: <offset>, LMA: <LMA>, Alignment: <alignment>, Flags: <SectionFlags>, Type: <SectionType>, Segments: <segments>
   # INSERT AFTER|BEFORE <anchor-section> # <script:line>
   <Rule_1>
   <Rule_2>
   ...
   <Rule_N>

The INSERT line is present only when the output section was positioned using
``INSERT AFTER`` or ``INSERT BEFORE`` in a linker script. The trailing script
context shows the originating linker script location.

<OutputSection> tells output section properties and contains a list
of :code:`Rule`s. Placement of rules conveys the placement of the rule contents
in the output image layout. The contents of *Rule_1* are placed before the
contents of *Rule_2* and so on. The contents of a rule is the merged content of
all the sections that were matched to the rule. Now, let's see what does
<Rule> consist of.

<Rule>
^^^^^^^

<Rule> (input section description) is represented as:

.. code-block:: bash

   <InputSectionDesc> #Rule <RuleNumber>, <InputFile>
   <InputSection_1>
   <InputSection_2>
   ...
   <InputSection_N>

:code:`RuleNumber` makes it easier to refer to a particular input
section description. <Rule> contains a list of <InputSection>
that got matched to this rule.

<InputSection>
^^^^^^^^^^^^^^^^

<InputSection> is typically represented as:

.. code-block:: bash

   <InputSectionName> <VMA> <size> <InputFile> #<SectionType>,<SectionFlags>,<SectionAlignment>
   <Symbol_1>
   <Symbol_2>
   ...
   <Symbol_N>

However, garbage-collected input sections are prefixed with '#', are
annotated with :code:`<GC>` and do not have :code:`<VMA>` component.
The <InputSection> for garbage-collected input sections
is represented as:

.. code-block:: bash

   # <InputSectionName> <GC> <VMA> <InputFile> #<SectionType>,<SectionFlags>,<SectionAlignment>
   <Symbol_1>
   <Symbol_2>
   ...
   <Symbol_N>


<Symbol>
^^^^^^^^^

<Symbol> is typically represented as::

   <VMA> <SymbolName>

However, garbage-collected symbols are prefixed with '#', and do not have
:code:`VMA` component. The <Symbol> for garbage-collected symbol is represented
as::

    # <SymbolName>

Example
^^^^^^^^

We have covered the simplified <OutputSectionAndLayout>
structure. Now, let's see a practical example.

**Foo.c**

.. code-block:: cpp

   int foo() { return 1; }

**Bar.c**

.. code-block:: cpp

   int bar() { return 3; }

**script.t**

.. code-block:: bash

   SECTIONS {
     FOO : { *(*foo*) }
     BAR : { *(*bar*) }
   }

Generate the map-file::

   hexagon-clang -o Foo.o Foo.c -c -ffunction-sections
   hexagon-clang -o Bar.o Bar.c -c -ffunction-sections
   hexagon-link -o FooBar.elf Foo.o Bar.o -T script.t -Map FooBar.map.txt

**Output Section and Layout excerpt from the map-file**

.. code-block:: bash

   # Output Section and Layout
   FOO     0x0     0xc # Offset: 0x1000, LMA: 0x0, Alignment: 0x10, Flags: SHF_ALLOC|SHF_EXECINSTR, Type: SHT_PROGBITS
   *(*foo*) #Rule 1, script.t [1:0]
   .text.foo       0x0     0xc     Foo.o   #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,16
           0x0             foo
   *(FOO) #Rule 2, Internal-LinkerScript (Implicit rule inserted by Linker) [0:0]

   BAR     0x10    0xc # Offset: 0x1010, LMA: 0x10, Alignment: 0x10, Flags: SHF_ALLOC|SHF_EXECINSTR, Type: SHT_PROGBITS
   *(*bar*) #Rule 3, script.t [1:0]
   .text.bar       0x10    0xc     Bar.o   #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,16
           0x10            bar
   *(BAR) #Rule 4, Internal-LinkerScript (Implicit rule inserted by Linker) [0:0]

We can see precisely how output sections are laid in the final layout and
what they are composed of. For instance, the :code:`FOO` output section
contains the :code:`.text.foo` input section because the :code:`.text.foo`
matches the rule :code:`*(*foo*)`. Under the input section information,
we can also see where each symbol of the section is placed in the output layout.

Other <OutputSectionAndLayout> substructures
---------------------------------------------

<LinkerScriptExpression>
^^^^^^^^^^^^^^^^^^^^^^^^^^^

<LinkerScriptExpression> entries are interspersed in <OutputSectionAndLayout>,
<OutputSection> and <Rule> and indicates their placement in the linker script.
<LinkerScriptExpression> contains the linker script expression
annotated with the values.

For example, if the linker script contains:

.. code-block:: bash

   SECTIONS {
     FOO : {
       v = . + 0x5;
       *(*foo*)
     }
   }

Then the map-file will contain:

.. code-block:: bash

   FOO     0x0     0x100c # Offset: 0x1000, LMA: 0x0, Alignment: 0x10, Flags: SHF_ALLOC|SHF_EXECINSTR, Type: SHT_PROGBITS, Segments : [ FOO ]
           v(0x5) = .(0x0) + 0x5; # v = . + 0x5; script.t
   *(*foo*) #Rule 1, script.t [1:0]
   .text.foo       0x0     0xc     1.eld.o        #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,16
           0x0             foo

Note the :code:`v(0x5) = .(0x0) + 0x5;` line under :code:`FOO` output section.

<Padding>
^^^^^^^^^^

<Padding> describes a padding in the output image layout. Paddings are either
requested explicitly by the user or are added automatically by the linker to
satisfy alignment constraints. Like <LinkerScriptExpression>, <padding> is
also interspersed in <OutputSectionAndLayout>, <OutputSection> and <Rule>.
The position of <padding> entry in the map-file conveys the position of padding
in the memory layout.

Let's see an example of explicitly requested padding.

If the linker script contains:

.. code-block:: bash

   SECTIONS {
    FOO : {
      . = . + 0x10;
      *(*foo*)
    }
   }

Then the map-file will contain:

.. code-block:: bash

   # Output Section and Layout

   FOO     0x0     0x1c # Offset: 0x1000, LMA: 0x0, Alignment: 0x10, Flags: SHF_ALLOC|SHF_EXECINSTR, Type: SHT_PROGBITS
   PADDING 0x0     0x10    0x0
           .(0x10) = .(0x0) + 0x10; # . = . + 0x10; script.t
   *(*foo*) #Rule 1, script.t [1:0]
   .text.foo       0x10    0xc     Foo.o   #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,16
           0x10            foo
   *(FOO) #Rule 2, Internal-LinkerScript (Implicit rule inserted by Linker) [0:0]

Please note the :code:`PADDING` details under the :code:`FOO` output section.

Link map in YAML format
----------------------------
    * If you use the MapStyle option to specify YAML-style output for the map file, the linker generates a YAML file instead of the text file that shows the memory map for the program.

    * To derive statistics from the YAML map file produced by the linker, use the **YAMLMapParser.py** tool and include any of the following options:

        * **–info=architecture** – List the architecture
        * **sizes** – List the code and data sizes for the objects in the image
        * **summarysizes** – List the code and data sizes of the image
        * **totals** – List the total size of all objects in the image
        * **unused** – List the sections eliminated from the image
        * **unusedsymbols** – List the symbols eliminated from the image
        * **--map** – Display the memory map of the images
        * **--xref** – List the cross references between input sections
        * **list** – Redirect the output to a file

     The following table describes the sections of a YAML file

     +------------------------+-----------------------------------------------------+
     |  Section               |  Description                                        |
     +========================+=====================================================+
     | Header                 | Top level information of the program that was built |
     +------------------------+-----------------------------------------------------+
     | Version                | Information Tools version information               |
     +------------------------+-----------------------------------------------------+
     | Archive Records        | Archive files pulled in by the linker               |
     +------------------------+-----------------------------------------------------+
     | Inputs                 | Inputs that were used                               |
     +------------------------+-----------------------------------------------------+
     | InputInfo              | Inputs that were used and not used                  |
     +------------------------+-----------------------------------------------------+
     |  Linker script used    | Linker script file that was used                    |
     +------------------------+-----------------------------------------------------+
     | BuildType              | Type of build                                       |
     +------------------------+-----------------------------------------------------+
     | OutputFile             | Name of the output file                             |
     +------------------------+-----------------------------------------------------+
     | EntryAddress           |  Entry address for the image built                  |
     +------------------------+-----------------------------------------------------+
     | CommandLine            | information on how the linker was called            |
     +------------------------+-----------------------------------------------------+
     | CommandLineDefaults    | Various defaults applied in the linker              |
     +------------------------+-----------------------------------------------------+
     | OutputSections         |      All output sections                            |
     +------------------------+-----------------------------------------------------+
     |  DiscardedComdats      | COMDAT C++ section groups that discarded            |
     +------------------------+-----------------------------------------------------+
     | DiscardedSections      | Sections discarded by garbage collection            |
     +------------------------+-----------------------------------------------------+
     | DiscardedCommonSymbols | Common symbols that were discarded                  |
     +------------------------+-----------------------------------------------------+
     |  LoadRegions           | Segments that the program loader will load          |
     +------------------------+-----------------------------------------------------+
     | CrossReferences        | Cross-reference table for the program               |
     +------------------------+-----------------------------------------------------+
