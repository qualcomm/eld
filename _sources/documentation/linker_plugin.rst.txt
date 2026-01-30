Linker Plugins
================

.. contents::
   :local:

Overview
----------
    * The ELD allows one or more linker plugins to be loaded and called during link time.

    * Develop the linker plug-ins to query the linker about objects being linked during link time, and to evaluate their properties.

    * You can also change the order and contents of output sections using this approach.

    * Use C++ to develop the linker plugins.

    * They are built as shared libraries that allow the linker to dynamically load at link time.

    * The plugins use a fixed API that allows you to communicate with the linker.

    * Plugins are currently supported on all architectures where the functionality is available.

    *  The Map file records any changes made by the plugin for the layout. This is a good place to determine how the layout was affected due to the plug-in algorithm.


Plugin Usage
-------------
    * Most developers use linker scripts, and the linker plugin functionality can therefore be exercised using linker scripts.

    * This is to promote ease of use and have less maintenance overhead.

    * It is also easier to integrate with existing builds.

    * The linker can also provide better diagnostics.

Linker wrapper
-----------------
    * Build a plugin as a dynamic library and interact with the linker using an opaque LinkerWrapper handle.

    * The linker wrapper exists as a shared library that you link with the user plugin.

    * The linkerwrapper is named **libLW.so** on Linux platforms and **LW.dll** on Windows platforms.


User Plugin Types
------------------
    * The intent of each user-developed plugin is associated with an appropriate interface.

    * You can implement the plugin algorithm by deriving it from one such interface.

    * You can use more than one interface in a single plugin.

    * Plugin chaining can be used interchangeably to denote when you are mixing more than one interface in a single plug-in.

    * The rest of this section might use PluginType interchangeably to denote this.

    * Currently, four interface types are available. More interface types will be added based on new use cases in the future.

    +----------------------------------+---------------------------+
    |Interface type                    | Header file               |
    +==================================+===========================+
    | Section iterator plugin type     | SectionIteratorPlugin.h   |
    +----------------------------------+---------------------------+
    | Chunk iterator plugin type       | ChunkIteratorPlugin.h     |
    +----------------------------------+---------------------------+
    | Control memory size plugin type  | ControlMemorySizePlugin.h |
    +----------------------------------+---------------------------+
    | Control file size plugin type    | ControlFileSizePlugin.h   |
    +----------------------------------+---------------------------+


LinkerScript changes
---------------------
    Linker plugins can be enabled using linker scripts. The linker script keyword uses a fixed syntax:

    ::

        <PluginType>("LibraryName", "PluginName" [, "PluginOptions"])

    - PluginType
        Corresponds to one of the available interface types.

    - LibraryName
        * Corresponds to the dynamic library that contains the plugin for the linker to load.

        * Uses the same linker semantics for linking to the linker in a library, allowing ease of use.

        * Uses the name of the library without the lib prefix on Linux and without the .so/.dll suffix on Linux/Windows, respectively

    - PluginName
        Specifies the name of the plugin. The linker queries the dynamic library to provide an implementation for the specified interface type.

    - PluginOptions
        Used to pass an option to the plug-in.

User Plugin Work Flow
----------------------
    Use the following steps to create a linker plugin.

    #. Determine the appropriate interface(s).

    #. Include the appropriate header file.

    #. Create a C++ class derived from one of the interface types.

    #. Associate the implementation of the interface to have a unique name.

    #. Build a shared library.

    #. Make the shared library report Plugin API version.

    #. Write a RegisterAll function to register the plugin(s).

    #. Write a getPlugin function to return the appropriate plugin when the linker queries with PluginName.

Linker Work Flow
-----------------
    * All user plugins that are loaded by the linker must be initialized properly before the plugin can communicate with the linker and perform the steps.

Linker Operation
------------------
The linker performs the following sequence of operations with respect to plugins:

    #. Parses the linker script and finds all plug-ins.
    #. Loads the library specified by LibraryName.
        a. Uses LD_LIBRARY_PATH on Linux.
        b. Uses a standard method for searching dynamic libraries in Windows.
    #. Calls the RegisterAll() API in the library, which registers all the plug-ins that are contained in the library.
    #. Queries the library using the getPlugin() API with PluginName.
            The library returns the appropriate object for the linker to use to run the plugin algorithm.
    #. Inspects the plug-in to verify that the linker script keyword and the object have the same interface type.
    #. Initializes the plug-in with any additional options provided.
    #. Passes the appropriate content to the plug-in expressed as data structures for that PluginType.
    #. Runs the plug-in algorithm.
            Because more than one plug-in can be used and because plug-ins can be chained, the linker unloads the library only after all user plug-ins have been called.
    #. Before the plug-in is unloaded, the linker calls a function to Destroy the plug-in.
            This is the last step before the library is unloaded.
    #. The linker then unloads the plug-in.

- Plugin tracing
         Trace the linker work flow with the following option:

          ::

            -trace=plugin

Plugin data structures
--------------------------
* Appropriate data structures are exchanged depending on the specified plug-in interface type.

* These data structures are also used to communicate with the linker and get appropriate information from the linker.

    **Section**
        Corresponds to an input section from an ELF file.
    **OutputSection**
        Corresponds to an OutputSection section in the LinkerScript or output ELF file.
    **Chunk**
        Corresponds to a piece of an input section. Examples:
        * Individual strings of a section that contains merged strings
        * Contents of an output section

    **Block**
        Corresponds to the content of the output section with corrected relocations.
    **Symbol**
        Corresponds to an ELF symbol.
    **Use**
        Corresponds to a relocation from a chunk or section.
    **LinkerScriptRule**
        Corresponds to a LinkerScriptRule in an OutputSection

LinkerWrapper commands
----------------------
The plug-in uses the LinkerWrapper to communicate and exchange information with the linker. Following are the LinkerWrapper commands.



    **getVersion**
      Returns the version of the LinkerWrapper as a string. This command is useful for diagnostics.
    **AllocateMemory**
        * Allocates memory that must live for the duration of the link.
        * The ControlMemorySize and ControlFileSize interface types are the most common users of this functionality.
    **getUses(Chunk)**
        Queries the linker to find out what a Chunk refers to. The API returns a vector of uses.
    **getUses(Section)**
        Queries the linker to find out what a Section refers to. The API returns a vector of uses.
    **getSymbol(SymbolName)**
        Gets more information from the linker for a symbol (specified by SymbolName).
    **getOutputSection(Section)**
        * Determines which OutputSection was chosen by the linker.
        * The OutputSection is usually chosen by the linker by matching rules in the linker script.

    **setOutputSection(Section, OutputSectionName)**
        * Places the Section into the specified OutputSection in the linker script.
        * This command allows linker script decisions to be overridden for that particular Section.
    **MatchPattern(Pattern, Name)**
        * Utility function that allows the plug-in to match a Glob Pattern with a string.
    **setLinkerFatalError**
        Used by the plug-in when it discovers that there is an unhandled case when trying to link input files to produce an output image.
    **resetError**
        Resets any error status

.. note:: Use the PluginADT.h file to learn about the APIs documented in the header file.

Linker plugin interface
-------------------------
Following is the plugin interface that the linker will use and provide implementation for the functions:

    .. code-block:: c

            class Plugin {
            public:
            /* Initialize the plugin with options specified */
            virtual void Init(std::string Options) = 0;
            /* The actual algorithm that will be implemented */
            virtual Status Run (bool Verbose)= 0;
            /* Linker will call Destroy, and the client can free up any data
            structures that are not relevant */
            virtual void Destroy()= 0;
            /* Returns the last error; a value of 0 means there was no error */
            virtual uint32_t GetLastError()= 0;
            /* Returns the error as a string */
            virtual std::string GetLastErrorAsString ()= 0;
            /* Returns the name of the plugin */
            virtual std::string GetName()= 0;
            /* Destructor */
            virtual -Plugin(){}
            };

Plugin interfaces
-------------------
The following interfaces are available currently.

SectionIterator interface
^^^^^^^^^^^^^^^^^^^^^^^^^^
     * Use the linker script keyword, **PLUGIN_ITER_SECTIONS**.

     * This interface allows you to process every input section from every object file by implementing processSection().

     .. code-block:: c

                class SectionIteratorPlugin : public Plugin {
                public:
                /* Section that the linker will call the client with */
                virtual void processSection(Section S)= 0;
                };

SectionMatcher interface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    * Use the linker script keyword, **PLUGIN_SECTION_MATCHER**.

    * This interface allows you to process every input section, and it allows the plugin to read any section.

    * You can use the interface to read metadata sections or read sections that were garbage collected by the linker.

    *  SectionMatcherPlugin differs from SectionIteratorPlugin in that it allows any section content to be read before assigning output sections.

    .. code-block:: c

            class SectionMatcherPlugin : public Plugin {
            public:
            /* Sections that the linker will call the client with */
            virtual void processSection(Section S) = 0;
            };

ChunkIterator interface
^^^^^^^^^^^^^^^^^^^^^^^^
    * Use the linker script keyword, **PLUGIN_ITER_CHUNKS**.
    * This interface allows you to process every input chunk in an output section using **PocessChunk()**.
    * The processed chunks are returned when the linker calls **getChunks()**.

    .. code-block:: c

            class ChunkIteratorPlugin : public Plugin {
            public:
            /* Chunks that the linker will call the client with */
            virtual void processChunk(Chunk C) = 0;
            virtual std::vector<Chunk> getChunks()= 0;
            };

ControlFileSize interface
^^^^^^^^^^^^^^^^^^^^^^^^^^^
    * Use the linker script keyword, **PLUGIN_CONTROL FILESZ**.

    * This interface allows you to process the output memory block contained in an output section using **AddBlocks()**.

    * The processed block is returned when the linker calls **GetBlocks()**.
    * An example of such a plugin is to take the memory block, compress it, and then return it to the linker.

    .. code-block:: c

            class ControlFileSizePlugin : public Plugin {
            public:
            /* Memory blocks that the linker will call the client with */
            virtual void AddBlocks(Block memBlock) = 0;
            /* Return memory blocks to the client */
            virtual std::vector<Block> GetBlocks()= 0;
            };


ControlMemorySize interface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    * Use the linker script keyword, **PLUGIN_CONTROL MEMSZ**.
    * This interface allows you to process the output memory block contained in an output section using **AddBlocks()**.
    * The processed block is returned when the linker calls **GetBlocks()**.

    .. code-block:: c

            class ControlMemorySizePlugin : public Plugin {
            public:
            /* Memory blocks that the linker will call the client with */
            virtual void AddBlocks(Block memBlock) = 0;
            /* Return memory blocks to the client */
            virtual std::vector<Block> GetBlocks()= 0;
            };

OutputSectionIterator interface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    * Use the linker script keyword, **PLUGIN_OUTPUT_SECTION_ITER**.
    * The interface is defined as below.

    .. code-block:: c

            class OutputSectionIteratorPlugin : public Plugin { public:
            /* OutputSection that the linker will call the client with */
            virtual void processOutputSection(OutputSection O)= 0;
            };

    * The interface allows the plugin to iterate over all output sections and their contents.
    * Contents of output sections are described by Rules.
    * The OutputSectionIterator is called at various times during the linking stages:
        * BeforeLayout
        * CreatingSections
        * AfterLayout

    * The state of the linker can be queried by calling a function **getState** in the LinkerWrapper.

    * Depending on the state of the linker, the plug-in can query OutputSections and its contents appropriately.

    * The OutputSectionIterator interface allows the plug-in to iterate over rules specified in the OutputSection and what sections are essentially contained in a LinkerScriptRule.

    * Depending on the state of the Linker, the contents of the LinkerScriptRule can be modified.
    * Modifications of LinkerScriptRule contents include changing the OutputSection for a Section, changing the OutputSection for a Chunk

    **BeforeLayout**
        * When the state of the linker is set to BeforeLayout, the plug-in can query each rule for its contents.
        * The contents of the Rule can only be Sections.
        * Call the setOutputSection function in the LinkerWrapper to set the Section point to a different OutputSection.
        * Finish with all the assignments to different output sections by calling
        * finishAssignOutputSections in the LinkerWrapper.

    **CreatingSections**
        * When the state of the linker is set to CreatingSections, the plug-in can query each rule for its contents. The contents of the Rule can only be Chunks.
        * Call APIs specified in the LinkerScriptRule to do either of the following:
            * addChunk – Add a Chunk to a LinkerScriptRule
            * removeChunk – Remove a Chunk from a LinkerScriptRule.
            * updateChunk – Replace Chunks in a LinkerScriptRule.

    **AfterLayout**
        When the state of the linker is set to AfterLayout, the plugin can query for the size of the OutputSection.
