Getting started
============================

.. contents::
   :local:

About the linker
-----------------
Linking is the process of collecting and combining various pieces of code and data into a single file that can be loaded (copied) into memory and executed. Linking is performed automatically by programs called linkers, which enable separate compilation.

Source
------

eld is available in the git repository:

    .. note:: git clone git://github.com/qualcomm/eld


Summary of the linker features
-------------------------------
* Most important features supported by GNU Linker
    * Development going on to add additional support
* LTO
    * Supports ThinLTO
    * Supports Full LTO
    * Supports Both flavors of LTO with linker scripts
* Supports Version scripts
    * Supports extensive usage of Linker scripts
* Better Diagnostics
    * YAML Map Files
    * Easier to read Text Map files
* Support for Linker Plugins
* --reproduce flag to easily reproduce the linking step.

What the linker can accept as input
----------------------------------------
The following types of files can be used as input to ld.eld:

* Object files
* Static Libraries
* Shared Libraries
* Symbol definition file
* Extern list
* Binary files


What the linker outputs
-------------------------

* ELF File
* A partially linked ELF object that can be used as input in a subsequent link step.
* Map file
    * YAML Map
    * Text Map
* Tar file
    * when using --reproduce flag
