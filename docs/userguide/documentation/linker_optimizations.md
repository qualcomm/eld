# Linker Optimization Features

> - Plugins
>   : More Information about linker plugins can be found at {doc}`linker_plugins_updated`
>
> - Garbage Collection
>
>   > - Unused function and data sections are garbage collected by linker when we provide **--gc-sections** option at the link time
>   > - Garbage collected sections can be printed by providing **--print-gc-sections** option
>   > - --gc-sections decides which input sections are used by examining symbols and relocations.
>   > - The section containing the entry symbol and all sections containing symbols undefined on the command-line will be kept, as will sections containing symbols referenced by dynamic objects.
>   > - Note that when building shared libraries, the linker must assume that any visible symbol is referenced.
>   > - If the linker performs a partial link (-r linker option), then you will need to provide the entry point using the -e / --entry linker option.

> - Identical Code Folding (ICF)
>
>   > - ICF folds equivalent input code sections into one canonical section to
>   >   reduce the output image size.
>   > - Use **--icf=all** or **--icf=safe** to enable folding, or
>   >   **--icf=none** to disable it. The default is `none`. At this stage,
>   >   `all` and `safe` use the same conservative eligibility rules.
>   > - The current implementation folds only non-empty, allocatable executable
>   >   `SHT_PROGBITS` input sections that have one fragment, no relocations, the
>   >   same output section, identical flags, size, and contents.
>   > - Sections that are writable, TLS, mergeable, link-order, group members,
>   >   fixed-address, or otherwise special are excluded. This keeps folding
>   >   conservative until relocation-aware ICF is available.
>   > - The first matching input section is retained as the canonical section.
>   >   Symbols that refer to folded sections are redirected to it.
>   > - Use **--print-icf-sections** to print each folded section and its
>   >   canonical representative. Output follows input discovery order.
>   > - ICF is disabled for partial (`-r`) links.
