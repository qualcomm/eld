Merging Similar Strings
===========================

.. contents::
   :local:

Information
-------------
When the user uses strings that appear either in output/passed as parameters, they all go in to
readonly strings. Since it is a string, they are terminated by \0. 

When the user uses constant data values that appear in function calls, they all
go into readonly merge sections. Since its a data value, they all are associated
with an entry size thats specified in the section.

To reduce the output memory foot print, they all need to be de-duped.

This mode would be disabled during Partial Linking.

Option
--------
We will have an option called -Os thats associated with the linker, the merge
strings option would be automatically turned on when the user uses this option.

Similarly there will be an option -Om (optimized for reduced memory usage), the
merge option will be disabled as part of that as well.

By default, the merge option is disabled.

Handling AM Sections
-----------------------

When the Linker needs to handle the AM section, they need to be split into
multiple fragments defined by the Entry Size of the section.

Each fragment is associated with a symbol, and that is merged similar to how
debug strings are merged.

The tricky part is to fix the relocations. 

Handling AMS Sections
-----------------------

When the Linker needs to handle the AMS section, they need to be split into
multiple fragments by looking at individual strings, that are terminated by \0.

Each fragment is associated with a symbol, and that is merged similar to how
debug strings are merged.

Relocations
-------------
The relocations that are created would need to know how the individual fragments
are split.

To fix the relocation so that its associated with a proper fragment, the
relocation offset and the addend information has to be modified so that they
point to 

a) The fragments when they have been split
b) Suffix/Prefix of the data thats used when the relocation is applied to the 'AM' section


Diagnostics
---------------
We need to print diagnostics on how many bytes were merged, and how much was
saved. 
