Linker Plugin Examples
************************

.. contents::
   :local:

This section introduces and explains plugin framework APIs in a hands-on
manner. The goal of these examples is to demonstrate how to use plugin
framework APIs, rather than to present practical plugin use cases. Each
plugin example introduces some new plugin framework APIs while keeping
the example short and simple.

Each example will include the plugin, instructions on how to build and run it, and
an analysis of the plugin's run. To build and run the plugin, you will need access
to the Hexagon toolchain. In the snippets below, the substitution `${HEXAGON}` refers
to the Hexagon toolchain installation path. Let's get started.

Exclude symbols from the output image symbol table
====================================================

This example plugin describes how to exclude symbols from the output image symbol table.
Note that the plugin does not completely remove symbols from the link process, it only
excludes symbols from the output image symbol table.

*ExcludeSymbols* plugin with self-contained documentation.

.. literalinclude:: examples/ExcludeSymbols/ExcludeSymbols.cpp
   :language: cpp

*ExcludeSymbols* plugin removes the symbols 'foo', 'fooagain', 'bar', and 'baragain',
if they exist, from the output image symbol table.

To build the plugin, run the following command::

   clang++-14 -o libExcludeSymbols.so ExcludeSymbols.cpp -std=c++17 -stdlib=libc++ -fPIC -shared \
   -I${HEXAGON}/include -L${HEXAGON}/lib -lLW

Now, let's see the effect of this plugin on a sample program.

**1.c**

.. literalinclude:: examples/ExcludeSymbols/1.c
   :language: cpp

**1.linker.script**

.. literalinclude:: examples/ExcludeSymbols/1.linker.script
   :language: bash

Now, let's build **1.c** with the *ExcludeSymbols* plugin enabled.

.. code-block:: bash

   export LD_LIBRARY_PATH="${HEXAGON}/lib:${LD_LIBRARY_PATH}"
   hexagon-clang -o 1.o 1.c -c -ffunction-sections -fdata-sections
   hexagon-link -o 1.elf 1.o -T 1.linker.script

We can list the symbols present in an object file using hexagon-readelf.

.. code-block:: bash

   hexagon-readelf -s 1.elf

You can observe in the symbol table output that 'foo', 'fooagain', 'bar', and 'baragain'
symbols has been removed from the symbol table as directed by the plugin.

Add Linker Script Rules
=========================

This example plugin describes how to add and modify linker script rules.
By doing so, you can perform section rule-matching at a more granular level
than what is possible through linker scripts.

:code:`LinkerScriptRule` is similar to an input section description in that
it has a corresponding output section. However, unlike an input section
description, it does not match input sections by pattern matching.
Instead, a plugin must manually match chunks to a :code:`LinkerScriptRule`
object. This generally involves moving chunks from one
:code:`LinkerScriptRule` object to another. It is important to remove a chunk
from the old :code:`LinkerScriptRule` object once it has been added to a
new one. It is an undefined behavior for a chunk to be part of multiple
:code:`LinkerScriptRule` objects.

Also, you must only move chunks from one :code:`LinkerScriptRule` object to
another in the *CreatingSections* link state. It is an undefined behavior to
move chunks in other link states.

The *AddRule* plugin moves chunks from :code:`foo` and :code:`bar` output
sections to the :code:`var` output section. Let's see how to create this
plugin.

*AddRule* plugin with self-contained documentation.

.. literalinclude:: examples/AddRule/AddRule.cpp
   :language: cpp

To build the plugin, run the following command::

   clang++-14 -o libAddRule.so AddRule.cpp -std=c++17 -stdlib=libc++ -fPIC -shared \
   -I${HEXAGON}/include -L${HEXAGON}/lib -lLW

Now, let's see the effect of this plugin on a sample program.

**1.c**

.. literalinclude:: examples/AddRule/1.c
   :language: cpp

**1.linker.script**

.. literalinclude:: examples/AddRule/1.linker.script
   :language: bash

Now, let's build 1.c with the *AddRule* plugin enabled.

.. code-block:: bash

   export LD_LIBRARY_PATH="${HEXAGON}/lib:${LD_LIBRARY_PATH}"
   hexagon-clang -o 1.o 1.c -c -ffunction-sections -fdata-sections
   hexagon-link -o 1.elf 1.o -T 1.linker.script -Map 1.map.txt

We can see the symbol to section mapping using hexagon-readelf.

.. code-block:: bash

   hexagon-readelf -Ss 1.elf

readelf output:

.. literalinclude:: examples/AddRule/ReadelfOutput.txt
   :language: bash

You can observe in the readelf output that the section :code:`var` contains
the :code:`foo` and the :code:`bar` symbols, which is exactly what we expected
from the plugin.

Reading INI Files Functionality
=================================

The plugin framework provides built-in support for the input/output operations
of INI files. Linker plugins, like any other tool, may require external options
and configurations. Traditionally, linker plugins use INI files for this
purpose. This plugin demo demonstrates how to use INI files to provide options
and configurations to a plugin. You can also use JSON, YAML, plain text, or any
other format you prefer. However, for consistency with other linker plugins,
I suggest using INI files for the plugin options and configurations.

The demo will feature the *PrintSectionsInfo* plugin. This plugin will print
section information for all input sections that match any of the section name
patterns specified in the plugin configuration file. The plugin will expect
the configuration file to in INI format.

*PrintSectionsInfo* plugin with self-contained documentation.

.. literalinclude:: demos/PrintSectionsInfo/PrintSectionsInfo.cpp
   :language: cpp


To build the plugin, run::

   clang++-14 -o libPrintSectionsInfo.so PrintSectionsInfo.cpp -std=c++17 -stdlib=libc++ -fPIC -shared \
   -I${HEXAGON}/include \
   -L${HEXAGON}/lib -lLW

Now, let's see the effect of this plugin on a sample program.

**1.c**

.. literalinclude:: examples/PrintSectionsInfo/1.c
   :language: cpp

**1.linker.script**

.. literalinclude:: examples/PrintSectionsInfo/1.linker.script
   :language: bash

**PluginConf.ini**

.. literalinclude:: examples/PrintSectionsInfo/PluginConf.ini
   :language: bash

Now, let's build 1.c with *PrintSectionsInfo* plugin enabled.

.. code-block:: bash

   export LD_LIBRARY_PATH="${HEXAGON}/lib:${LD_LIBRARY_PATH}"
   hexagon-clang -o 1.o 1.c -c -ffunction-sections -fdata-sections
   hexagon-link -o 1.elf 1.o -T 1.linker.script

The plugin gives the below output::

   .text.bar
   Input file: 1.o
   Section index: 4
   Section alignment: 16

   .data.i
   Input file: 1.o
   Section index: 6
   Section alignment: 8

   COMMON.arr
   Input file: CommonSymbols
   Section index: 0
   Section alignment: 8

As expected, the plugin prints the section information of the sections whose
names match a pattern specified in the plugin configuration file,
*PluginConf.ini*.

Modify Relocations
=======================

The *ModifyRelocations* example plugin demonstrates how to inspect and modify
relocations. This plugin modifies the relocation symbol from
:code:`HelloWorld` to :code:`HelloQualcomm`. Additionally, it prints the
relocation source section and symbol for each relocation it iterates over.

To inspect and modify relocations, *ModifyRelocations* uses
:code:`LinkerPluginConfig`. :code:`LinkerPluginConfig` provides a callback
hook for relocations. This callback hook function is called for each
relocation that is of a registered relocation type. Relocation types can be
registered using :code:`LinkerWrapper::registerReloc`.

*ModifyRelocations* plugin with self-contained documentation.

.. literalinclude:: examples/ModifyRelocations/ModifyRelocations.cpp
   :language: cpp

To build the plugin, run::

   clang++-14 -o libModifyRelocations.so ModifyRelocations.cpp -std=c++17 -stdlib=libc++ -fPIC -shared \
   -I${HEXAGON}/include -L${HEXAGON}/lib -lLW

Now, let's see the effect of this plugin on a sample program.

**1.c**

.. literalinclude:: examples/ModifyRelocations/1.c
   :language: cpp

**1.linker.script**

.. literalinclude:: examples/ModifyRelocations/1.linker.script
   :language: cpp

Now, let's build 1.c with *ModifyRelocations* plugin enabled.

.. code-block:: bash

   export LD_LIBRARY_PATH="/local/mnt/workspace/partaror/llvm-project-formal/obj/bin:${LD_LIBRARY_PATH}"
   hexagon-clang -o 1.elf 1.c -ffunction-sections -fdata-sections -Wl,-T,1.linker.script

Running the above commands gives the below output::

   Relocation callback. Source section: .text, symbol: .start
   ...
   ...
   Relocation callback. Source section: .text.main, symbol: HelloWorld
   Relocation callback. Source section: .text.main, symbol: printf
   ...

This output is emitted by the plugin when it's iterating over relocations
of registered types.

Now, let's try running the generated binary image, *1.elf*::

   $ hexagon-sim ./1.elf

   hexagon-sim INFO: The rev_id used in the simulation is 0x00008d68 (v68n_1024)
   Hello Qualcomm!

   Done!
           T0: Insns=5479 Packets=2946
           T1: Insns=0 Packets=0
           T2: Insns=0 Packets=0
           T3: Insns=0 Packets=0
           T4: Insns=0 Packets=0
           T5: Insns=0 Packets=0
           Total: Insns=5479 Pcycles=8841

*Hello World!* has been replaced by *Hello Qualcomm!*. It's the result of
plugin changing the relocation symbol from :code:`HelloWorld` to
:code:`HelloQualcomm` for all :code:`R_HEX_B22_PCREL` relocations.

Section Rule-Matching
=======================

This example plugin describes how to modify section rule-matching using a
plugin. A plugin can create section overrides using the
:code:`LinkerWrapper::setOutputSection` API. Section overrides created by a
plugin override linker script section rule-matching. Plugins must call
:code:`LinkerWrapper::finishAssignOutputSections` after all section
overrides have been created. :code:`LinkerWrapper::finishAssignOutputSections`
brings the section override change into effect.

Section overrides must only be used in the *BeforeLayout* link state. After
the *BeforeLayout* state, chunks from input sections get merged into
output sections, making section overrides meaningless.

The *ChangeOutputSection* plugin sets the output section of :code:`.text.foo`
to :code:`bar`. That's it. Let's see how to create this plugin.

*ChangeOutputSection* plugin with self-contained documentation.

.. literalinclude:: examples/ChangeOutputSection/ChangeOutputSection.cpp
   :language: cpp

To build the plugin, run the following command::

   clang++-14 -o libChangeOutputSection.so ChangeOutputSection.cpp -std=c++17 -stdlib=libc++ -fPIC -shared \
   -I${HEXAGON}/include -L${HEXAGON}/lib -lLW

**1.c**

.. literalinclude:: examples/ChangeOutputSection/1.c
   :language: cpp

**1.linker.script**

.. literalinclude:: examples/ChangeOutputSection/1.linker.script
   :language: bash

Now, let's build 1.c with the *ChangeOutputSection* plugin enabled.

.. code-block:: bash

   export LD_LIBRARY_PATH="${HEXAGON}/lib:${LD_LIBRARY_PATH}"
   hexagon-clang -o 1.o 1.c -c -ffunction-sections -fdata-sections
   hexagon-link -o 1.elf 1.o -T 1.linker.script -Map 1.map.txt

Now, let's see the output section of :code:`foo`.

.. code-block:: bash

   hexagon-readelf -Ss 1.elf

hexagon-readelf output:

.. literalinclude:: examples/ChangeOutputSection/ReadelfOutput.txt
   :language: bash

You can observe from the readelf output that the :code:`bar` section contains
the :code:`foo` symbol. This means the output section of :code:`.text.foo` was
indeed changed to :code:`bar`.

That's all well and good, but how do we see the nice annotation we added when
calling :code:`LinkerWrapper::setOutputSection`? All plugin actions, along
with their annotations, are recorded in the map file. The text map file can be
used to quickly view plugin actions and the annotations.

To see all the plugin action information, you can either view the text map
file and find the "Detailed Plugin information" component, or run the command
below to directly view the information from the text map file.

.. code-block:: bash

   less +/"Detailed Plugin information" 1.map.txt

Output

.. literalinclude:: examples/ChangeOutputSection/PluginActionsInformation.txt
   :language: bash

Change Symbol Value to Another Symbol
===========================================

The *ChangeSymbolValue* plugin demonstrates how to change the value of a
symbol to another symbol. Specifically, it changes the value of the
:code:`HelloWorld` symbol to the value of the :code:`HelloQualcomm` symbol.
It also attempts (unsuccessfully) to change the value of the
:code:`HelloWorldAgain` symbol to the value of the :code:`HelloQualcommAgain`
symbol. We will see why this happens.

Now, let's see how to create the *ChangeSymbolValue* plugin.

The *ChangeSymbolValue* plugin with self-contained documentation.

.. literalinclude:: examples/ChangeSymbolValue/ChangeSymbolValue.cpp
   :language: cpp

To build the plugin, run::

   clang++-14 -o libChangeSymbolValue.so ChangeSymbolValue.cpp -std=c++17 -stdlib=libc++ -fPIC -shared \
   -I${HEXAGON}/include -L${HEXAGON}/lib -lLW

Now, let's see the effect of this plugin on a sample program.

**1.c**

.. literalinclude:: examples/ChangeSymbolValue/1.c
   :language: cpp

**1.linker.script**

.. literalinclude:: examples/ChangeSymbolValue/1.linker.script
   :language: bash

Now, let's build 1.c with *ChangeSymbolValue* plugin enabled.

.. code-block:: bash

   export LD_LIBRARY_PATH="${HEXAGON}/lib:${LD_LIBRARY_PATH}"
   hexagon-clang -o 1.elf 1.c -ffunction-sections -fdata-sections -Wl,-T,1.linker.script

Running the above commands produces the following output::

   'HelloWorld' symbol value has been successfully reset to the value of 'HelloQualcomm' symbol.
   Symbol value resetting failed for 'HelloWorldAgain'.

The above output is printed by the plugin. It indicates whether the resetting
of symbol values was successful or not. So, why was the resetting of
'HelloWorldAgain' not successful? :code:`LinkerWrapper::resetSymbol` can only
resets the value of the symbols that do not already have a value.
:code:`LinkerWrapper::resetSymbol` silently fails and returns false if the
symbol requested to be reset already has a value.

Now, let's run the generated **1.elf** binary::

   $ hexagon-sim ./1.elf

   hexagon-sim INFO: The rev_id used in the simulation is 0x00008d68 (v68n_1024)
   Hello Qualcomm!
   Hello again World!

   Done!
           T0: Insns=6810 Packets=3558
           T1: Insns=0 Packets=0
           T2: Insns=0 Packets=0
           T3: Insns=0 Packets=0
           T4: Insns=0 Packets=0
           T5: Insns=0 Packets=0
           Total: Insns=6810 Pcycles=10677

As expected, the output first prints, 'Hello Qualcomm!', as the value of the
:code:`HelloWorld`` symbol successfully got reset to the value of
:code:`HelloQualcomm`. The output then prints, 'Hello again World!', as the
resetting of :code:`HelloWorldAgain`` to :code:`HelloQualcommAgain`` was
unsuccessful.