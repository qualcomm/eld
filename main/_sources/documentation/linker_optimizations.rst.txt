Linker Optimization Features
============================

    * Plugins
        More Information about linker plugins can be found at :doc:`linker_plugin`

    * Garbage Collection

        * Unused function and data sections are garbage collected by linker when we provide **--gc-sections** option at the link time

        * Garbage collected sections can be printed by providing **--print-gc-sections** option

        * --gc-sections decides which input sections are used by examining symbols and relocations.

        * The section containing the entry symbol and all sections containing symbols undefined on the command-line will be kept, as will sections containing symbols referenced by dynamic objects.

        * Note that when building shared libraries, the linker must assume that any visible symbol is referenced.

        * If the linker performs a partial link (-r linker option), then you will need to provide the entry point using the -e / --entry linker option.