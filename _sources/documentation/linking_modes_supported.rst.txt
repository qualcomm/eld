Supported Linking Modes
========================

.. contents::
   :local:

A linking model is a group of command-line options and memory maps that control the behavior of the
linker.
The linking models supported by eld are:

Static Linking
-----------------
In this mode, all the symbol definitions/functions are copied in the final executable.

Dynamic Linking
-----------------
In Dynamic linking is accomplished by placing the name of a sharable library in the executable image. Actual linking with the library routines does not occur until the image is run, when both the executable and the library are placed in memory. An advantage of dynamic linking is that multiple programs can share a single copy of the library.

Partial Linking
-----------------
This model produces a relocatable ELF object suitable for input to the linker in a subsequent
link step. The partial object can be used as input to another link step. The linker performs
limited processing of input objects to produce a single output object

PIE
--------------
This model produces a Position Independent Executable (PIE). This is an executable
that does not need to be executed at a specific address but can be executed at any suitably
aligned address. All objects and libraries linked into the image must be compiled to be position
independent

Shared Libraries
------------------------
A shared library is an object module that, at run time, can be loaded at an arbitrary memory address and linked with a program in memory. The process is known as dynamic linking and is performed by a program called a dynamic linker. Shared libraries are also referred to as shared objects (.so).

Shared libraries are "shared" in two different ways:

there is exactly one .so file for a particular library. The code and data in this .so file are shared by all of the exe obj files that reference the library.
a single copy of the .text section of a shared library in memory can be shared by different running processes.