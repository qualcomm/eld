Linker Plugins
*****************

.. contents::
   :local:

What and why of linker plugins
================================

Linker plugins make it possible to run user-defined code during the link
process. Implemented as a C++ class, they are a programmatical way to add
new functionalities and modify default linker behavior.

But why modify default linker behavior? We will now discuss the long answer
we promised at the beginning of the linker plugins documentation.

Plugins provide more control and flexibility over the output image layout as
compared to the linker scripts.
For complicated rules, linker scripts are often very cumbersome to write,
sometimes desired rule matching behavior might even be impossible from just
linker script. For example, a plugin can move around chunks from one output
section to another. Linker plugins also allow the user to alter program layout
dynamically which is not possible using linker script.

Plugins add custom behavior to the linker by defining behavior for the hooks
that the linker runs in various stages of the link process. These hooks allows
plugins to modify the default linker behavior, provide a fine-grained control
over the output image layout, add new functionalities to the linker.

This guide will describe all that you need to know about linker plugins,
and how to effectively write one yourself. This guide assumes that the reader
has a basic understanding of linkers, linker relocations and linker scripts.

Elements of the link process
===============================

Before we move further, let's first quickly go over some important elements of
the link process. A solid understanding of these elements will help to better
understand the link process, and linker's capabilities and limitations. This
in turn will help to effectively design and write plugins.

Symbols
--------

Formally, a symbol is a name associated with a value. This value can be
an offset within a section, a virtual memory address, or simply an absolute value.
Symbols are an integral component of an object file. Among other things, they are
used to refer to global variables and functions in a program.

.. note::

   Global variables were specifically mentioned because local variables are created and managed
   on the stack at run-time and linker is blissfully unaware of them. Beware: local symbols
   are not the same as local variables!


:code:`eld::plugin::Symbol` represents a linker symbol.

Symbol resolution
^^^^^^^^^^^^^^^^^^^

For each group of non-local symbols with the same name, symbol resolution
selects one of these symbols using well-defined rules. The selected symbol
is part of the output image symbol table and is used to resolve symbol
references to that name.

Input Section
---------------

Each relocatable object module, *M*, has input sections. Input sections
contains most of the object file information for the linking view:
instructions, data, symbol table, relocation information and so on.

:code:`eld::plugin::Section` represents an input section.


Output Section
----------------

Output section contains one or more input sections. The output sections are
in turn mapped to a segment. A segment contains one or more output sections,
and is used for finally loading the program. Typically, output sections with
same *Alloc*, *Exec*, and *Write* permissions are mapped to the same segment.

:code:`eld::plugin::OutputSection` represents an output section.

Chunk
--------

In ELD's terminology, a section is made up of many sub-parts called as
chunk or fragment. Plugins can move chunks from one output section to another.
Linker scripts do not have this level of control over the output image layout.

:code:`eld::plugin::Chunk` is a handler for a chunk. It can be used to inspect the
properties, symbols and content of a chunk.

Relocation
-----------

Relocation is the process of connecting symbolic references with symbolic
definitions. All symbolic references of the same symbol should be connected
to the same definition. Object files contain relocation entries that
describes how the relocations should be processed.

:code:`eld::plugin::Use` represent relocations.

Section Mapping
-----------------

Each input section is either discarded or is mapped to some output section.
Linker has some default mapping rules that defines which input section is to
be mapped to which output section. Traditionally, linkers allowed users to
add their own custom mapping rules using linker script :code:`SECTIONS`
command.

Now, we have covered the basic elements of the link process. Let's dive
into the linker plugin functionality.

Plugin Types
=============

ELD has different plugin types. Plugin types differ in which hooks they provide.
Hooks are pre-defined spots in the link pipeline where a plugin can tap into the
linker to customize the link. Plugin taps into the linker by defining the callback
function for the hooks. The linker calls these callback functions at the hook sites.
For brevity, we will refer callback function for the hook as hook callback function.

There two distinct kinds of plugins: LinkerPlugin plugin and layout plugins.
LinkerPlugin plugin is more general and provides hook that covers the entire
link process. On the other hand, the layout plugins have specialized hooks for
iterating over certain link components (input sections, output sections, ...)
instead of having general hooks. Different layout plugin types iterate over
different link components. For example, :code:`SectionMatcher` plugin type provide
hooks to process input sections and :code:`OutputSectionIterator` plugin type
provides hooks to process output sections.

There are a total of 6 plugin types. One LinkerPlugin plugin type and 5 layout plugin types.
For each plugin type, there is a corresponding plugin class. Plugin authors create a new
plugin by inheriting from the corresponding plugin class and implementing the hook
callback functions.



The 6 plugin interfaces along with their corresponding C++ class
and header file are listed below.

.. list-table::
   :widths: 33 33 34
   :header-rows: 1

   * - Plugin interface
     - Plugin interface class
     - Header file
   * - LinkerPlugin
     - LinkerPlugin
     - ELD/PluginAPI/LinkerPlugin.h
   * - SectionMatcher
     - SectionMatcherPlugin
     - ELD/PluginAPI/SectionMatcherPlugin.h
   * - SectionIterator
     - SectionIteratorPlugin
     - ELD/PluginAPI/SectionIteratorPlugin.h
   * - ControlFileSize
     - ControlFileSizePlugin
     - ELD/PluginAPI/ControlFileSizePlugin.h
   * - ControlMemorySize
     - ControlMemorySizePlugin
     - ELD/PluginAPI/ControlMemorySizePlugin.h
   * - OutputSectionIterator
     - OutputSectionIteratorPlugin.h
     - ELD/PluginAPI/OutputSectionIteratorPlugin.h

Linker Wrapper
====================

Running a linker plugin
=========================

There are two methods to run linker plugins:

- Adding a plugin invocation command in a linker script. *(Not supported for :code:`LinkerPlugin` plugins)*
- Specifying plugin configuration file using `--plugin-config` option. *(Recommended way)*

We call these methods as *plugin load specification* because they specify
how the linker should load a plugin.

Adding a plugin invocation command in a linker script.
--------------------------------------------------------

::

   <PluginTypeKeyword>("LibraryName", "PluginName" [, "PluginOption"])

For output section plugin types, :code:`ControlMemorySizePlugin` and
:code:`ControlFileSizePlugin`, users should add the plugin invocation command to the
output section description of the output section on which these plugins
should run.
For example::

  SECTIONS {
    output_section <PluginTypeKeyword>("LibraryName", "PluginName" [, "PluginOption"]) : {
      *(.text)
    }
  }

.. note::

   Only one output section plugin can be attached to an output section.

- **PluginTypeKeyword**

   Each plugin type has a corresponding linker script plugin
   keyword.

   .. list-table::
      :widths: 50 50
      :header-rows: 1

      * - Plugin interface type
        - Linker script keyword
      * - SectionMatcher
        - PLUGIN_SECTION_MATCHER
      * - SectionIterator
        - PLUGIN_ITER_SECTIONS
      * - ControlFileSize
        - PLUGIN_CONTROL_FILESZ
      * - ControlMemorySize
        - PLUGIN_CONTROL_MEMSZ
      * - OutputSectionIterator
        - PLUGIN_OUTPUT_SECTION_ITER

- **LibraryName**

   - Name of the dynamic library that contains the plugin for the linker to
     load.
   - Finds the library in the same search paths as if the library was passed
     as an input to the linker.
   - Uses the name of the library without the lib prefix on Linux and without
     the .so/.dll suffix on Linux/Windows, respectively

- **PluginName**

   Name of the plugin. The linker queries the dynamic library to provide an
   implementation of the plugin with the name *PluginName* for the specified
   interface type.

- **PluginOption**

   It can be used to pass an option to the plugin.

Specifying plugin configuration file using :code:`--plugin-config` option
-----------------------------------------------------------------------------

:code:`--plugin-config` option takes a plugin configuration yaml file.
Plugin configuration file should contain a :code:`GlobalPlugins` list.
Elements of the :code:`GlobalPlugins` list defines which plugins should be
loaded. Each value of the list is an object containing all the information
required to load and initialize a specific plugin.

Plugin configuration file format should be as follows:::

  ---
  GlobalPlugins:
    - Type: PluginInterfaceType
      Name: PluginName
      Library: LibraryName
      Options: PluginOption

  OutputSectionPlugins:
    - OutputSection : OutputSectionName
      Type : PluginInterfaceType
      Name : PluginName
      Library : LibraryName
      Options : PluginOption

:code:`GlobalPlugins` list can specify any number of elements.
:code:`Options` member is optional.

:code:`Library` name should be specified without the lib prefix on Linux
and without the .so/.dll suffix on Linux/Windows

:code:`ControlMemorySizePlugin` and :code:`ControlFileSizePlugin` are output
section plugins. Therefore, in the plugin configuration file, they need to be
added to :code:`OutputSectionPlugins` list.

.. note::

   Only one output section plugin can be attached to an output section.


User Plugin Workflow
=======================

The following steps describe how to develop a plugin:

1) Determine the appropriate plugin interfaces for your plugin.

  A plugin type can be one of :code:`LinkerPlugin`, :code:`SectionIterator`,
  :code:`SectionMatcher`, :code:`OutputSectionIterator`,
  :code:`ControlMemorySize`, and :code:`ControlFileSize`.

  A plugin should ideally follow the unix-philosophy of doing one thing well.
  You should generally try to avoid inheriting from multiple plugin interfaces
  if possible. It generally leads to more complicated code, and a plugin that
  does more than one thing. But that being said, if you do think
  functionalities of multiple plugin interfaces will improve your plugin's
  design and readability, then you should go ahead with inheriting
  from multiple plugins.

  You can also create and chain multiple plugins for your task, similar in
  ideology to how unix utilities operate, where action of one plugin depends
  on the action and result of the previous one.

1) Create a C++ source file that will contain your plugin(s).

  Plugins source file needs to include the headers of the plugin interface(s)
  that the plugin(s) inherits from. Plugin interfaces header files are
  contained in, :code:`${HEXAGON_TOOLCHAIN}/Tools/include/ELD/PluginAPI`.

3) Create a C++ class that inherits from the appropriate plugin interface(s) for each plugin.

4) Override virtual functions.

  Virtual functions are the means the linker works with the plugins.
  Each plugin interface provides hooks at various place in the linker
  codebase, the hooks functionalities are implemented by overriding
  virtual functions. There are also other virtual functions that
  do not implement functionalities for the hooks, but are required to
  work with the plugin infrastructure. Example of these virtual functions are:
  :code:`Plugin::GetName`, :code:`Plugin::GetLastError`,
  :code:`Plugin::GetLastErrorAsString` etc.

5) Associate the plugins with unique names.

6) Build a shared library for the source file.

  In a unix environment, shared library for the plugins can be created as::

    # Compile the plugins source file
    clang++ -c -I${HEXAGON_TOOLCHAIN_ROOT}/Tools/include ${SOURCE_BASENAME}.cpp -fPIC -stdlib=libc++

    # Link the plugin library with linker wrapper library, LW.
    clang++ -shared ./${SOURCE_BASENAME}.o -L${HEXAGON_TOOLCHAIN_ROOT}/Tools/lib -lLW -stdlib=libc++ -o lib${SOURCE_BASENAME}.so

7) Define :code:`RegisterAll` function in C linkage.

  :code:`RegisterAll` function goal is to register the plugins contained
  in the file. Registering a plugin here simply means to initialize
  a plugin object.

  Linker calls this function for each plugin library. This function should
  creates a plugin object for each plugin that the library contains.

8) Define a :code:`getPlugin(const char *pluginName)` function in C linkage.

  :code:`getPlugin(const char* plugin)` function goal is to return the
  requested plugin. Each plugin in the plugin source file should be uniquely
  identifiable by a name.

  Linker calls this function at the time of loading plugins to get a plugin
  object of plugin named :code:`pluginName`.

9) Make the plugin report its API version

  Linker verifies linker plugin compatibility. Linker will report an error
  and refuse to load a plugin if it determines that it is not compatible.

  Linker plugin compatibility is determined using Linker API version.
  Plugin API version consists of major and minor version numbers.
  A plugin is compatible with the linker if plugin's major API version is
  equal to the linker major API version and plugin's minor API version is
  equal or lower to the linker's minor API version. The major API version
  is incremented when a existing functionality in the API is changed in an
  incompatible way. Such changes are expected to be rare. The minor API version
  is incremented when new functionality is added to the Plugin API. Note that
  Plugin API versions are distinct from linker release version numbers. The
  current linker Plugin API version is reported by the :code:`--version`
  command line option.

  For the versioning mechanism to work, each plugin must report its API version
  by exporting the :code:`getPluginAPIVersion(unsigned int *Major, unsigned int *Minor)`
  function (with C linkage). For convenience, this function is defined in the
  header file :code:`PluginVersion.h` and including this file in one of the
  plugin's C++ source files will automatically make the plugin report its correct
  API version.

  Starting with version 19.0, each plugin must report its API version. Linker
  will refuse to load a plugin that does not report its version.

10) (Optionally) Define a :code:`getPluginConfig(const char *pluginName)` in C linkage to return the configuration object of the requested plugin.

11) (Optionally) Define a :code:`Cleanup` function in C linkage.

  This function is called at the time unloading plugins. This function
  generally deletes any allocated memory that was required for the lifetime
  of the plugin library.


How the linker runs plugin
============================

Linker performs the following operations to load, run and unload plugins.

1) Finds all the plugins, from linker script and plugins configuration file, that needs to be attached to the link process.

2) Loads all the specified plugin libraries.

   #. To find plugin libraries, :code:`LD_LIBRARY_PATH` environment variable is used on unix environment.
   #. Standard method for searching dynamic libraries is used in Windows.

3) Calls :code:`RegisterAll` function from each plugin library. This function
   should ideally create the plugin objects for each plugin defined in the
   library.

4) Calls :code:`getPlugin(const char *pluginName)` function for each plugin.

   This function returns an appropriate plugin object for *pluginName* plugin.
   This object is used to run the plugin.

5) Calls :code:`getPluginConfig(const char *pluginName)` function, if available, for each plugin.
   This function returns an appropriate plugin configuration object for *pluginName* plugin.

6) Inspects the plugin to verify that the plugin load specification and plugin type have same plugin interface type.

7) Initializes each plugin at their *Init* hook point by calling
   :code:`Plugin::Init(std::string Option)` virtual function. *Option*
   argument is optionally specified by the plugin user at the plugin load specification.

8) Calls :code:`Cleanup` function, if available, for each plugin library.
   This function should ideally delete any allocated memory that was required
   for the lifetime of the plugin library.

9) Unload the plugins and the plugin libraries.

Tracing plugins
===================

Plugin workflow can be traced using the option :code:`--trace=plugin`.

This option informs the linker to emit detailed diagnostics for
all the plugins.

A sample trace output is shown below::

  ...
  ...
  Note: Registration function found RegisterAll in Library libDiagOpt.so
  Note: Plugin handler getPlugin found in Library libDiagOpt.so
  Note: Cleanup function found Cleanup in Library libDiagOpt.so
  Note: Plugin Config function getPluginConfig found in library libDiagOpt.so
  Note: Registering all plugin handlers for plugin types
  Note: Found plugin handler for plugin type DIAGRELOCATION in Library libDiagOpt.so
  Note: Initializing Plugin libDiagOpt.so, requested by Plugin DIAGRELOCATION having Name DIAGOPT
  ...
  ...
  Note: Plugin DIAGRELOCATION Destroyed

  <-------------------------> DiagOpt::Destroy() AfterLayout<------------------------->
  Note: Unloaded Library libDiagOpt.so.16


Link States
==================

.. graphviz:: ../images/LinkStates.dot
   :alt: Link states sequence
-------------------------------------

The link process has different run states, also known as link states. The
actions that a plugin can perform at any given time depend on the current link
state. Many actions are only meaningful for specific link states. Understanding
what happens in each link state will help determine which actions can be
performed in each state. Therefore, a thorough understanding of linker and
link states is crucial for writing a linker plugin. Different link states
are described below:

Initializing
---------------

The linker begins with the *Initializing* link state. In this state, the
linker reads input files, initializes standard sections and configurations,
and read relocations. Not much of interest happens during this state.

Let's move on to the more happening states.

BeforeLayout
---------------

After the *Initializing* state, the linker enters the *BeforeLayout*
link state. Here, it performs section rule matching, garbage collection,
updates input sections with attributes (such as KEEP) present in the
linker script, and updates the linker cache-file, among other things.


CreatingSections
-----------------

After the *BeforeLayout* state, the linker enters the *CreatingSections*
link state. Here, it transfers the contents (chunks) of input sections
into output sections and finalizes the layout and symbols.

Note that the section rule-matching is already complete before the
*CreatingSections* state, therefore, plugins cannot alter the section
rule-matching in *CreatingSections* state or any subsequent state.

AfterLayout
---------------

After the *BeforeLayout* state, the linker enters *Afterlayout* link state.
Here, it finalizes the relocations and writes the output object file.

In this link state, a plugin can compute output image layout checksum using
:code:`eld::plugin::LinkerWrapper::getImageLayoutChecksum`. A plugin cannot perform
any action that tries to modify the image layout now.

Deep Dive into the Plugin Types
========================================

There are two distinct kinds of linker plugins: LinkerPlugin and layout plugins.
LayoutPlugins focus on simplifying the modification of specific layout components,
such as input and output sections. In contrast LinkerPlugins are more general, uniform and
offer more functionalitier than the layout plugins.

PluginBase class
------------------

This is the base plugin class for all the plugin interface classes. It
provides common functionalities to all the plugin interfaces. It is a pure virtual
class and thus cannot be instantiated / directly used.

.. doxygenclass:: eld::plugin::PluginBase
   :members:

Please note, that even though the virtual functions -- :code:`Plugin::Init`,
:code:`Plugin::Run` and :code:`Plugin::Destroy` are available in
all the plugin interfaces, they map to different hooks in the linker for
different plugin interfaces. For example, :code:`Run` hook of
:code:`SectionIteratorPlugin`` is not equivalent to the :code:`Run` hook of
:code:`OutputSectionIteratorPlugin` even though in both cases, the hooks
functionality is defined by overriding the :code:`Run` virtual function.
For the base :code:`Plugin`, these functions are not mapped to any hook in
the linker.

LinkerPlugin Type
------------------------------

:code:`LinkerPlugin` is the most versatile and the recommended plugin type for
creating new plugins. It has hooks that covers the entire linker flow and offers
more functionality than the layout plugins. :code:`LinkerPlugin` has hooks of the two forms:

- :code:`ActBefore<LinkState>`
- :code:`Visit<LinkComponent>`

The :code:`ActBefore<LinkState>` hooks are called *just* before the linker enters
the link state :code:`LinkState`. The :code:`Visit<LinkComponent>` hooks are called
*immediately* after the linker creates a link component (input section, symbol, ...).
For example, :code:`VisitSymbol(eld::plugin::Symbol S)` is called immediately
after the symbol is created.


.. graphviz:: ../images/LinkerPluginFlow.dot
   :alt: LinkerPlugin flow
--------------------------------------------

Section Matcher Interface
---------------------------

:code:`SectionMatcherPlugin` interface iterates input sections. It provides
four hooks: :code:`Init`, :code:`ProcessSection`, :code:`Run` and
:code:`Destroy`. To define the behavior of these hooks, the functions
:code:`Plugin::Init`, :code:`SectionMatcher::ProcessSection`
and :code:`Plugin::Destroy` must be overridden.

:code:`ProcessSection(Section S)` callback hook function is called for each
input section.

:code:`SectionMatcherPlugin` is called into action after the linker reads input
files and performs section rule-matching, but before
garbage collection. As a consequence, garbage-collection information
cannot be accessed during
:code:`SectionMatcherPlugin` run. Link state throughout the plugin run is
:code:`eld::plugin::LinkerWrapper::BeforeLayout`.

.. graphviz:: ../images/SectionMatcherFlow.dot
   :alt: SectionMatcherPlugin flow.
----------------------------------------------

:code:`UserPlugin::Init` is called before :code:`UserPlugin::ProcessSection`,
and :code:`UserPlugin::Run` and :code:`UserPlugin::Destroy` are called
subsequently after processing input sections, as described in the figure above.

Linker script keyword for :code:`SectionMatcherPlugin` interface is
:code:`PLUGIN_SECTION_MATCHER`. Therefore, to run a
:code:`SectionMatcherPlugin` plugin using a linker script, add the following
line to the linker script::

   PLUGIN_SECTION_MATCHER("LibraryName", "PluginName" [, "PluginOption"])

Section Iterator Interface
-----------------------------
:code:`SectionIteratorPlugin` interface iterates over input sections.
It provides four hooks: :code:`Init`, :code:`ProcessSection`, :code:`Run`
and :code:`Destroy`. To define the behavior of these hooks, the functions
:code:`Plugin::Init`, :code:`Plugin::ProcessSection`,
:code:`SectionIterator::ProcessSection` and :code:`Plugin::Destroy`
must be overridden.

:code:`ProcessSection(Section S)` callback hook function is not called for
garbage collected input sections. However, it is called for discarded
input sections.

:code:`SectionIteratorPlugin` is called into action after the linker performs
section rule-matching and garbage collection. As a consequence, section rule
matching and garbage collection information is accessible during
:code:`SectionIteratorPlugin` run. Link state throughout the plugin run is
:code:`eld::plugin::LinkerWrapper::BeforeLayout`.

.. graphviz:: ../images/SectionIteratorFlow.dot
   :alt: SectionIteratorPlugin flow
-----------------------------------------------

:code:`UserPlugin::Init` is called before :code:`ProcessSection`,
and :code:`UserPlugin::Run` and :code:`UserPlugin::Destroy` are called
after :code:`ProcessSection`, as described in the figure above.

Linker script keyword for :code:`SectionIteratorPlugin` interface is
:code:`PLUGIN_ITER_SECTIONS`. Therefore, to run a
:code:`SectionIteratorPlugin` plugin using a linker script, add the following
line to the linker script::

   PLUGIN_ITER_SECTIONS("LibraryName", "PluginName" [, "PluginOption"])

Output Section Iterator Interface
------------------------------------

:code:`outputSectionIteratorPlugin` interface iterates over all the
output sections. It provides four hooks: :code:`Init`,
:code:`ProcessOutputSection`, :code:`Run` and :code:`Destroy`.
To define the behavior of these hooks, the functions :code:`Plugin::Init`,
:code:`Plugin::ProcessSection`,
:code:`OutputSectionIteratorPlugin::ProcessOutputSection` and
:code:`Plugin::Destroy` must be overridden. :code:`OutputSectionIteratorPlugin`
is called at different link states:

  - BeforeLayout
  - CreatingSections
  - AfterLayout

All the plugin hooks are called for each of the link states. That means,
each of :code:`Init`, :code:`Run` and :code:`Destory`
are called 3 times over the complete plugin run. And
:code:`ProcessOutputSection` is called three times for each
output section over the complete plugin run.

The link state can be queried via :code:`LinkerWrapper::getState` member function.

Linker script keyword for :code:`OutputSectionIteratorPlugin` interface is
:code:`PLUGIN_OUTPUT_SECTION_ITER`. Therefore, to run a
:code:`SectionIteratorPlugin` using a linker script, add the following line
to the linker script::

  PLUGIN_OUTPUT_SECTION_ITER("LibraryName", "PluginName" [, "PluginOption"])

Different phases of :code:`OutputSectionIteratorPlugin` are described below:

BeforeLayout -- Phase 1
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. graphviz:: ../images/OutputSectionBeforeLayoutFlow.dot
   :alt: OutputSectionIteratorPlugin flow in BeforeLayout link state
--------------------------------------------------------------------

This phase is called into action after the linker has performed section
rule-matching and before the linker has performed section merging.

CreatingSections -- Phase 2
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. graphviz:: ../images/OutputSectionCreatingSectionsFlow.dot
   :alt: OutputSectionIteratorPlugin flow in CreatingSections link state
------------------------------------------------------------------------

This phase is called into action after the linker has performed section
merging.

AfterLayout -- Phase 3
^^^^^^^^^^^^^^^^^^^^^^^^^

.. graphviz:: ../images/OutputSectionAfterLayoutFlow.dot
   :alt: OutputSectionIteratorPlugin flow in AfterLayout link state.
----------------------------------------------------------------------------

This phase is called into action after the linker has finalized the image
layout and before the linker has written the output object file.

ControlMemorySizePlugin
-------------------------

This interface allows the plugin to add input sections to the output image.
These newly added input sections are generally, but not necessary, based on
some already existing output section.

:code:`ControlMemorySizePlugin` is an output section plugin. Plugins of this
type has to explicitly mark each output section that it needs to run on. Since
this is an output section plugin, :code:`init` and :code:`destroy` functions are also
called for each output section the plugin runs on. This plugin is called in the
`LinkerWrapper::CreatingSections` step of the linker.

.. graphviz:: ../images/ControlMemorySizePluginFlow.dot
   :alt: ControlMemorySizePlugin Flow
-------------------------------------------------------

:code:`init`, :code:`AddBlocks`, :code:`Run`, :code:`GetBlocks` and :code:`Destroy`
functions are called for each output section.

Brief description of :code:`ControlMemorySizePlugin` hooks:

:code:`init`
^^^^^^^^^^^^^^
This hook initializes the plugin for the output section.

:code:`AddBlocks`
^^^^^^^^^^^^^^^^^^^

This hook gives access to the current output section to the plugin.
:code:`Block` parameter of the hook function contains the current
output section. It is generally used in the creation of the new block
which linker can use as an additional input section.

:code:`Run`
^^^^^^^^^^^^

This hook is generally used to process the current output section, and create
any new sections that linker should use as additional input sections.

:code:`GetBlocks`
^^^^^^^^^^^^^^^^^^^

This hook returns a vector of :code:`Block`. These blocks are used by the
linker as additional input sections.

:code:`Destroy`
^^^^^^^^^^^^^^^^

This hook is used to perform any required clean-up.

Linker script keyword for ControlMemorySize interface is
:code:`PLUGIN_CONTROL_MEMSZ`. Therefore, to run a SectionIterator plugin
on an output section, :code:`.out` using a linker script, add the following
line to the output section description of :code:`.out` in the linker script::

   SECTIONS {
      .out PLUGIN_CONTROL_MEMSZ("LibraryName", "PluginName" [, "PluginOption"]) : { *(.out) }
    }
