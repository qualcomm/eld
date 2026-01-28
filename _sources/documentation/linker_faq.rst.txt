=================================================
Linker support and frequently asked questions!!!
=================================================

.. contents::
   :local:

General Linker Questions
=========================

How do I pass arguments to linker using clang/clang++ driver ?
---------------------------------------------------------------

clang/clang++ driver provides **"-Wl,<args>"** option to pass args to linker.

-Wl,<args>

Pass the comma separated arguments in args to the linker

E.g. clang  <compiler_options> **-Wl,-Map=test.map,-e=main**

where -Map and -e are linker specific options/args.

These args will be passed to the underlying linker used for processing.

Alternatively, user can also use **"-Xlinker <arg>"** option to pass argument one
at a time to linker.

-Xlinker <arg>

Pass arg to the linker.

E.g. clang  <compiler_options> **-Xlinker -Map=test.map -Xlinker -e=main**

How to remove unused functions from the final ELF ?
----------------------------------------------------

This can be achieved in two ways:

* With LTO
    * All source files should be compiled with compiler option "-flto".
    * During linking stage, enable LTO optimization by specifying "-flto" option.

      E.g. clang <compiler_options> **-flto -c** foo.c -o foo.o

      .. note::
        When -flto option is used during compilation, the output object file
        (say, foo.o) is not in ELF format.It will be in LLVM-IR bitcode binary
        format.

      .. code-block:: bash

        clang <compiler_options> -flto -c bar.c -o bar.o
        ld.eld <other_linker_options> -flto foo.o bar.o -e <entry_point> -o final.out

* Without LTO
    * All source files should be compiled with compiler option "-ffunction-sections".
    * During linking stage, enable garbage collection by specifying "--gc-sections" option.
      Example:

      .. code-block:: bash

           clang <compiler_options> -ffunction-sections -c foo.c -o foo.o
           clang <compiler_options> -ffunction-sections -c bar.c -o bar.o
           ld.eld <other_linker_options> --gc-sections -e <entry_point> foo.o bar.o -o final.out

How do I trace a symbol ?
--------------------------

The user can trace a symbol with --trace-symbol <symbolname> or --trace=symbol=<symbolname>

Example:

.. code-block:: cpp

    $ cat 1.c
    int foo() {return 0;}
    int bar() {return 1;}
    $ cat 2.c
    int baz() {return foo() + bar ();}
    $ ld.eld 1.o 2.o --trace-symbol bar # equivalent to --trace=symbol=bar
    Note: Symbol `bar' from Input file `1.o' with info `(ELF)(FUNCTION)(DEFINE)[Global]{DEFAULT}' being added to Namepool
    Note: Symbol `bar' from Input file `2.o' with info `(ELF)(NOTYPE)(UNDEFINED)[Global]{DEFAULT}' being added to Namepool
    Note: Symbol `bar' from Input file `1.o' with info `(ELF)(FUNCTION)(DEFINE)[Global]{DEFAULT}' being resolved from Namepool
    Symbol bar, application site: 0x2c

How do I trace a relocation ?
------------------------------

The user can trace a relocation with the option --trace=reloc="<relocname>"

Figure out garbage collected symbols
-------------------------------------

The user can find the list of sections that the linker garbage collected using
the option *--print-gc-sections* option.

The user then needs to go over the sections in the input files, and list the
symbols using objdump tool.

Linker is garbage collecting a symbol
---------------------------------------

The linker can garbage collect the symbol only if the symbol is not referenced.
You can use the option --trace=live-edges and see if you are finding a reference
to the section that contains the symbol.

If you don't see the section that contains the symbol, there might be a missing
link.

You might want to use the option

  * --entry=<entry symbol>
  * Use the KEEP linker script directive to add to the list of sections that needs
    to be kept.
  * -u *symbol_name* to keep a symbol from being garbage collected

Print Timing Information
--------------------------

Linker can print timing information with the option --print-timing-stats.
This option can be used to see where the linker is spending most of its time.

Linker is taking a long time to link
--------------------------------------

The linker may be taking a long time to link due to the following patterns

* A lot of linker script rules
    * Linker script rules with EXCLUDE_FILE
* A lot of object files
* Bad image layout

For figuring out any of the above, you should check the Map file and see the
statistics when the link is successful.

How to figure out why a archive member is being loaded by the linker
----------------------------------------------------------------------

If you are trying to figure out why an archive member is being loaded you need
to look at the archive records.

Example:

.. code-block:: bash

    cat > 1.c << \!
    int foo() { return bar(); }
    !

    cat > 2.c << \!
    int bar() { return baz(); }
    !

    cat > 3.c << \!
    int baz() { return bar(); }
    !


    clang -target hexagon -c 1.c -ffunction-sections
    clang -target hexagon -c 2.c 3.c -ffunction-sections
    hexagon-ar cr mylib.a 2.o 3.o
    hexagon-link 1.o mylib.a -Map x

If you see the map file section for archive reference records, you will find
this information :-

.. code-block:: bash

    Archive member included because of file (symbol)
    mylib.a(2.o)
                    1.o (bar)
    mylib.a(3.o)
                    mylib.a(2.o) (baz)

You can interpret the information as below :-

* There was a reference in 1.o for bar, hence mylib.a(2.o) was loaded.
* There was a reference in 2.o for baz, hence mylib.a(3.o) was loaded.

Controlling Page size
----------------------

You can use the option z max-page-size=<x> to set the page size used by the
linker.

How to fail a build for linker warnings
----------------------------------------

A linker warning may be considered fatal when the switch --fatal-warnings is
used.

You can turn off this behavior --no-fatal-warnings or removing the switch
--fatal-warnings.

Weak references and definitions
--------------------------------

Symbols can be given weak binding by the compiler and assembler. Weak references
and definitions are typically references to library functions.

The linker does not load an object from a library to resolve a weak reference.
It is able to resolve the weak reference only if the definition is included in
the image explicitly.

This can be done by specifying the object file containing the definition on the
link command line.

Alternatively, using "--whole-archive <archive-file> --no-whole-archive" linker
options includes all objects files in the archive to the link.

Also, removing the weak attribute on the symbol will make it a normal or non-weak.
For non-weak symbols, linker will scan the library/archive and loads the
required member(s).

.. note::

    An unresolved weak function call is replaced with a no-operation instruction,
    NOP (only for aarch64 and if the linker has not reserved a PLT).

**An example showing that the linker does not load an object from a library to
resolve a weak reference.**

By inspecting the disassembly of "a.out", user can observe that call to bar()
in main() is replaced with a NOP.

.. code-block:: cpp

    cat > 1.c << \!
    __attribute__((weak))  int bar();
    int main() {
     bar();
     return 0;
    }
    !

    cat > 2.c << \!
    int bar() { return 0; }
    !

    clang -c 1.c 2.c -target aarch64
    rm -f lib2.a
    llvm-ar cr lib2.a 2.o
    ld.eld 1.o lib2.a -t -march aarch64

Reproducing a failure
----------------------

If you are having an issue, and you want to pass the link step for someone else
to debug, you can use the *--reproduce <tarball>*  option.

The *–-reproduce <tarball>*  option creates a tar ball with all the inputs that
were fed to the linker, and rewrites the link command to make it easy for users
to reproduce the problem.

.. note::

    If the tarball is big and you want the linker to compress the tarball
    automatically, you can use the switch -reproduce-compressed.

.. note::

    If you are having a link time failure and the problem does not reproduce everytime,
    but in specific builds, it may be because of non determinism in the builds.
    In that case use the option --reproduce-on-fail.

The reproduce-on-fail switch only creates a tarball when the link step fails.

Multiple ways to invoke ELD linker
------------------------------------

* arm-link can be invoked since it is a symbolic link to ld.eld

.. code-block:: bash

    > cat bar.c
    int main(){return 0;}

    clang -target  armv7-none-eabi  -g  -c bar.c  -o bar.o
    arm-link -m armelf_linux_eabi -dynamic-linker /lib/ld-linux.so.3 -o bar.elf --strip-debug bar.o

* Pass -fuse-ld=eld flag to clang driver, so that eld is used for linking

.. code-block:: bash

    clang -target armv7-none-eabi -fuse-ld=eld -fuse-baremetal-sysroot bar.c -o bar.elf -fno-use-baremetal-crt

.. note::

    ELD linker flags can be passed to clang driver using -Wl prefix.
    Example : clang ...... -Wl,-shared -Wl,-gc-sections bar.o -o bar.elf

* Pass -v (verbose) when linking using clang binary, pick the link command line,
  edit and execute it

.. code-block:: bash

  $ clang -target armv7-none-eabi -fuse-ld=eld -fuse-baremetal-sysroot bar.o -o bar.elf -fno-use-baremetal-crt -v
  clang version 16.0.0
  Snapdragon LLVM ARM Compiler 16.0.0
  Target: armv7-none-unknown-eabi
  Thread model: posix
  InstalledDir: /pkg/qct/software/llvm/release/arm/16.0.0/bin
   "/pkg/qct/software/llvm/release/arm/16.0.0/bin/ld.eld" -EL -X --eh-frame-hdr -m armelf_linux_eabi -dynamic-linker /lib/ld-linux.so.3 -o bar.elf bar.o -L/pkg/qct/software/llvm/release/arm/16.0.0/armv7-none-eabi/libc/lib -L/afs/localcell/cm/gv2.6/sysname/pkg.@sys/qct/software/llvm/release/arm/16.0.0/lib/clang/16.0.0/lib/baremetal --start-group -lc -lclang_rt.builtins-armv7 --end-group -lc

  $ ld.eld -EL -X --eh-frame-hdr -m armelf_linux_eabi -dynamic-linker /lib/ld-linux.so.3 -o bar.elf bar.o -L/pkg/qct/software/llvm/release/arm/16.0.0/armv7-none-eabi/libc/lib -L/afs/localcell/cm/gv2.6/sysname/pkg.@sys/qct/software/llvm/release/arm/16.0.0/lib/clang/16.0.0/lib/baremetal --start-group -lc -lclang_rt.builtins-armv7 --end-group -lc --strip-debug

Ignore multiple definition errors and continue with the link
--------------------------------------------------------------

Sometimes a developer might want to ignore all multiple definition errors and
continue with the link.

This can be achived by using --allow-multiple-definition switch.

Example:

.. code-block:: bash

  cat > 1.c << \!
  int foo() { return 0; }
  !

  cat > 2.c << \!
  int foo() { return 1; }
  !

  #compile
  clang -target hexagon -c 1.c 2.c
  #produces error
  hexagon-link 1.o 2.o
  #produces a successful link
  hexagon-link 1.o 2.o --allow-multiple-definition

Multiple ways to provide entry point to linker
-----------------------------------------------

* Using linker flag : -e <value>   --> Name of entry point symbol
* Specifying ENTRY(symbol) command in a linker script
* Initialising the value of linker symbol "start"
* Specifying the start address for the first input section in linker script
  (eg: .text : AT(0))

How to obtain a non-executable stack
--------------------------------------

Non-executable stack (NX) is a virtual memory protection mechanism to block
shell code injection from executing on the stack by restricting a particular
memory and implementing the NX bit.

ELD has the following equivalent option :

**-z noexecstack** : Mark the executable as not requiring an executable stack

What is the difference between section type NOBITS and PROGBITS
-----------------------------------------------------------------

NOBITS section do not occupy size on the disk.

PROGBITS section occupies space on disk.

How to link libraries to resolve circular dependencies
--------------------------------------------------------

The order in which the libraries are loaded matter. Incorrect order leads to
"undefined reference" errors.

Keeping the libraries to linked within "--start-group" and "--end-group" linker
flags takes care of the circular dependencies, so that the user need not worry
about the order in which libs are loaded.

Circular Dependies:

.. code-block:: bash


  cat > 1.c << \!
  extern int bar();
  int foo() { return bar(); }
  !

  cat > 2.c << \!
  extern int fred();
  int bar() { return fred(); }
  !

  cat > 3.c << \!
  int baz() { return 0; }
  !

  cat > 4.c << \!
  extern int baz();
  int fred() { return baz(); }
  !

  clang -target hexagon -c 1.c 2.c 3.c 4.c
  # lib2.a creates a dependency of fred in lib4.a in a different group
  hexagon-ar cr lib2.a 2.o 3.o
  # lib3.a creates a reverse-dependency to lib2.a which was previously visited
  hexagon-ar cr lib3.a 4.o
  # problem
  hexagon-link 1.o --start-group lib2.a --end-group --start-group lib3.a --end-group
  # solution 1
  hexagon-link 1.o --start-group lib2.a --end-group --start-group lib3.a lib2.a --end-group
  # solution 2
  hexagon-link 1.o --start-group 2.o 3.o --end-group --start-group lib3.a --end-grou

How to disable use of standard system startup files
-----------------------------------------------------

"-nostartfiles" : Does not use the standard system startup files when linking.

If this option is specified, user has to specify their own program entry point
and write their own start files

How to disable use of standard system libraries
-------------------------------------------------

"-nostdlib" : Disable default search path for libraries

This option will prevent the compiler from picking the standard libraries,
rtlibs provided by the toolchain. User has to come up with their own definitions
or libs so that there are no undefined reference errors reported.

The compiler rt libraries have to be specified explicitly if there are calls
to __aeabi* functions and are not defined by the users.

How to remove a section from a OBJ file
-----------------------------------------

llvm-objcopy tool can be used to remove a section from an ELF file.

.. Example::
  llvm-objcopy --remove-section=<section_name>

How to extract OBJ files from an archive
------------------------------------------

llvm-ar is the utility to extract obj files from the archive

.. Example::
  llvm-ar -x <archive_file>

How to obtain a deterministic output from linker
--------------------------------------------------

By default deterministic behaviour is supported. To get maximum throughput from
linker, --enable-threads=all linker option must be passed.

How to obtain memory statistics from an ELF file or OBJ file
--------------------------------------------------------------

llvm-size utility can provide details of size of .text, .data, .rodata, .bss, etc.

How to compare layout of two output images
--------------------------------------------

Mapfile contains the layout of the image. You can compare the layout section of
mapfiles of the two output images which you are interested in.

By default, mapfiles layout section also contains virtual addresses associated
with the layout. This brings in too much unnecessary noise when comparing the two
layouts, because a small change in the layout can result in change of virtual
address of many sections. This can be fixed by using '-MapDetail only-layout'
option.

.. code-block:: bash

  // Run link commands with '-MapDetail only-layout' when generating the two output images.
  vim -d image1.map image2.map # To compare the two output images layouts.

How to embed a binary and use it in 'C' code
----------------------------------------------

You can include a binary file and use it in 'C' code to reference the binary content.

Here is an example which builds a simple table and embeds that table in 'C' code.

Simple python code to create a binary file
-------------------------------------------

The below python code when run creates a binary file by name table.bin

.. code-block:: python

  f = open('table.bin', 'w+b')
  byte_arr = [1,2,3,4,5]
  binary_format = bytearray(byte_arr)
  f.write(binary_format)
  f.close()

Create a simple assembly file to include this binary file
-----------------------------------------------------------

This snippet of assembly creates a section named table and includes

.. code-block:: bash
  .section "table","a",@progbits
  .incbin "table.bin"

Include this assembly file to build a static executable
---------------------------------------------------------

The program snippet below prints the contents of the binary file to stdout.

.. code-block:: cpp
  extern char __start_table;
  extern char __stop_table;
  int main() {
    char *s = &__start_table;
    char *e = &__stop_table;
    while (s < e) {
      printf("%d\n", *s);
      s++;
    }
    return 0;
  }

Linker defines a variable __start_<section> and __stop_<section> if the linker
is going to create an output section that matches a 'C' identifier.

Using the snippet above, and a simple assembly file, you will be able to iterate
over the binary file in 'C' code

Where should .note.gnu.property section map to in elf
-------------------------------------------------------

The .note.gnu.property section is mapped to PT_NOTE segment by default which
should have only read permission.

How to garbage-collect symbols in a shared library?
-----------------------------------------------------

One way to garbage-collect symbols in a shared library is to mark the symbols as
hidden. Hidden symbols are garbage collected from a shared library if they are
not used from any of the entry symbols in the shared library (entry symbols are
symbols that are marked with visibility default).

Example:

.. code-block:: bash

  cat >1.c <<\EOF
  // can not be garbage-collected
  int foo() {
    return 1;
  }

  // can be garbage-collected
  __attribute__((visibility("hidden")))
  int bar() {
    return 2;
  }

  int baz() {
    return 0;
  }
  EOF

  clang -target hexagon -o 1.o 1.c -c -fPIC -ffunction-sections
  hexagon-link -o 1.elf 1.o --gc-sections --print-gc-sections -e baz

Running the above script gives the below output:

.. code-block:: bash

  cat
  clang -target hexagon -c 1.c -ffunction-sections -fPIC
  hexagon-link 1.o -shared --gc-sections --print-gc-sections -e baz
  Trace: GC : 1.o[.text]
  Trace: GC : 1.o[.text.foo]

We can see that 'foo', a hidden symbol, got garbage-collected during the link
process as it was not reachable from the entry point (baz).

How are zero-sized sections handled?
-------------------------------------

How are zero-sized sections handled?

* A relocation refers to the zero sized section
* A symbol is present in the zero sized section

.. Note::

  Releases

  This change is observed on the below toolchain / patch releases on Hexagon :-
    * Hexagon 8.7.04
  Releases that will be published after summer of 2023, will also have this support.

Relocation referring to a zero sized section
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In the below example, the section .text.baz has size zero and is a relocation
target for .text.c1, hence .text.baz is useful and is made a part of the layout.

Example:

The example illustrates a reference to the section .text.baz references by the
section .text.c1

.. code-block:: bash

  cat > x.s << \EOF
  .section .text.foo
  .global foo
  .set foo, bar
  .section .text.bar
  bar:
  .word 100
  .section .text.baz
  .section .text.c1
  .word .text.baz
  .section .text.empty
  EOF

  cat > main.c << \EOF
  int main() { return foo(); }
  EOF

  cat > script.t << \EOF
  SECTIONS {
   . = 0x10000;
   .foo : {
    *(.text.*)
    *(.text)
   }
  }
  EOF

Compile and Link step

.. code-block:: bash

  $ clang -c x.s  main.c
  $ ld.eld main.o x.o -T script.t -Map x

Image layout

The image layout is adjusted to keep **.text.baz** in the output, as there is
a relocation that is referred from the section .text.c1

.. code-block:: bash

  # Output Section and Layout
  .(0x10000) = 0x10000; # . = 0x10000; script.t

  .foo    0x10000 0x24 # Offset: 0x1000, LMA: 0x10000, Alignment: 0x10, Flags: SHF_ALLOC|SHF_EXECINSTR, Type: SHT_PROGBITS
  *(.text.*) #Rule 1, script.t [9:0]
  .text.foo       0x10000 0x0     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,1
  .text.bar       0x10000 0x4     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,1
          0x10000         bar
          0x10000         foo
  .text.baz       0x10004 0x0     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,1
  .text.c1        0x10004 0x4     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,1
  *(.text) #Rule 2, script.t [2:0]
  PADDING_ALIGNMENT       0x10008 0x8     0x0
  .text   0x10010 0x14    main.o  #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,16
          0x10010         main
  *(.foo) #Rule 3, Internal-LinkerScript (Implicit rule inserted by Linker) [0:0]

The section has associated symbols
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sections containing symbols are preserved in the layout. Such sections are not
considered to be empty sections.

.. warning::
  Such cases should be avoided at all times due to the following reasons

    * The code is very sensitive to layout for it to work properly.
    * When link time garbage collection is enabled, this may end up garbage
      collecting sections which would rather be needed at runtime.

Example:

.. warning::
  In the below example

  * The code is very sensitive to layout for it to work properly because the
    program relies on .text.foo and .text.bar to be placed together
  * When link time garbage collection is enabled, this may end up garbage
    collecting sections which would rather be needed at runtime

     * In the example below the linker may end up garbage collecting .text.bar

.. code-block:: bash

  cat > x.s << \!
  .section .text.foo
  .type foo, @function
  .global foo
  foo:
  .section .text.bar
  .type bar, @function
  .global bar
  bar:
  nop
  !

  cat > main.c << \!
  int main() { return foo(); }
  !

  cat > script.t << \!
  SECTIONS {
   . = 0x10000;
   .text: {
    *(.text.*)
    *(.text)
   }
  }
  !

Compile and link step

.. code-block:: bash

  $ clang -c x.s  main.c
  $ ld.eld main.o x.o -T script.t -Map x

Image Layout

.. code-block:: bash

  # Output Section and Layout
  .(0x10000) = 0x10000; # . = 0x10000; script.t

  .text   0x10000 0x24 # Offset: 0x1000, LMA: 0x10000, Alignment: 0x10, Flags: SHF_ALLOC|SHF_EXECINSTR, Type: SHT_PROGBITS
  *(.text.*) #Rule 1, script.t [6:0]
  .text.foo       0x10000 0x0     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,1
          0x10000         foo
  .text.bar       0x10000 0x4     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,1
          0x10000         bar
  *(.text) #Rule 2, script.t [2:0]
  PADDING_ALIGNMENT       0x10004 0xc     0x0
  .text   0x10010 0x14    main.o  #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,16
          0x10010         main
  *(.text) #Rule 3, Internal-LinkerScript (Implicit rule inserted by Linker) [0:0]

Can a zero-sized section ever affect the layout?  - Yes it can
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Example:

.. code-block:: bash

  cat > x.s << \EOF
  .section .text.foo
  .global foo
  .set foo, bar
  .section .text.bar
  bar:
  .word 100
  .section .text.baz
  .p2align 4
  .section .text.baz1
  .section .text.baz2
  .section .text.c1
  .word .text.baz
  .section .text.empty
  EOF

  cat > main.c << \EOF
  int main() { return foo(); }
  EOF

  cat > script.t << \EOF
  SECTIONS {
      . = 0x10003;
      .baz :  {
          . = 0x10005;
          *(.text.baz1)
          . = . + 16;
          *(.text.baz)
          . = . + 28;
          *(.text.baz2)
          . = . + 40;
      }
      .foo : {
          *(.text.foo)
          *(.text*)
      }
  }
  EOF

Compile and Link Step:

.. code-blck:: bash

  $clang -c x.s  main.c
  $ld.eld main.o x.o -T script.t -Map x.map

Image Layout

**Image Layout from xmap:**

.. code-block:: bash

  # Output Section and Layout
  .(0x10003) = 0x10003; # . = 0x10003; script.t

  .baz    0x10010 0x54 # Offset: 0x1010, LMA: 0x10010, Alignment: 0x10, Flags: SHF_ALLOC|SHF_EXECINSTR, Type: SHT_PROGBITS
          .(0x10005) = 0x10005; # . = 0x10005; script.t
  *(.text.baz1) #Rule 1, script.t [1:0]
          .(0x100010015) = .(0x100010005) + 0x10; # . = . + 0x10; script.t
  *(.text.baz) #Rule 2, script.t [1:0]
  PADDING_ALIGNMENT       0x10015 0xb     0x0
  .text.baz       0x10020 0x0     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,16
          .(0x1003c) = .(0x10020) + 0x1c; # . = . + 0x1c; script.t
  *(.text.baz2) #Rule 3, script.t [1:0]
  PADDING 0x100010005     0x10    0x0
  PADDING 0x1003c 0x28    0x0
          .(0x10064) = .(0x1003c) + 0x28; # . = . + 0x28; script.t
  *(.baz) #Rule 4, Internal-LinkerScript (Implicit rule inserted by Linker) [0:0]

  .foo    0x10070 0x1c # Offset: 0x1070, LMA: 0x10070, Alignment: 0x10, Flags: SHF_ALLOC|SHF_EXECINSTR, Type: SHT_PROGBITS
  *(.text.foo) #Rule 5, script.t [1:0]
  .text.foo       0x10070 0x0     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,1
  *(.text*) #Rule 6, script.t [9:0]
  .text   0x10070 0x14    main.o  #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,16
          0x10070         main
  .text.bar       0x10084 0x4     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,1
          0x10084         bar
          0x10084         foo
  .text.c1        0x10088 0x4     x.o     #SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,1


*Here because of p2align directive for .text.baz the PADDING_ALIGNMENT of size 0xb
at address 0x100015 was added*

Debugging zero-sized sections related issues
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ELD has 2 ways that help in debugging issues wrt to zero-sized sections

* Linker Diagnostics as warnings tied to option -Wall & -Wzero-sized-sections
  * Please look for traces containing "Warning: Zero sized fragment" for
    zero-sized section-related warnings
  .. code-block:: bash

    cat > x.s << \EOF
    .section .text.baz
    .type baz, %function
    .global baz
    baz:
    .section .text.bar
    .type bar, %function
    .global bar
    bar:
    nop
    .section .foo,"a",%progbits
    .local sym
    sym:
    .size sym, 40
    EOF

    cat > main.c << \EOF
    int main() { return baz(); }
    EOF

    cat > script.t << \EOF
    SECTIONS {
     . = 0x10000;
     .text: {
      *(.text.*)
      *(.text)
     }
    }
    EOF

    $ clang -c x.s  main.c
    $ ld.eld main.o x.o -T script.t -Map x
    Warning: Zero sized fragment .foo for non zero sized symbol sym from input file x.o

* Linker stats emitted in the map file as
  * ZeroSizedSections
  * ZeroSizedSectionsGarbageCollected

.. code-block:: bash

  cat > x.s << \!
  .section .text.foo
  .global foo
  .set foo, bar
  .section .text.bar
  bar:
  .word 100
  .section .text.baz
  .section .text.c1
  .word .text.baz
  .section .text.empty
   !

  cat > main.c << \!
  int main() { return foo(); }
  !

  cat > script.t << \!
  SECTIONS {
   . = 0x10000;
   .text: {
    *(.text.*)
    *(.text)
  }
  }
  !

  $ clang -c x.s  main.c
  $ ld.eld main.o x.o -T script.t -Map x
  $ cat x
  ...
  # LinkStats Begin
  # ObjectFiles : 2
  # LinkerScripts : 2
  # ThreadCount : 64
  # NumInputSections : 29
  # ZeroSizedSections : 17
  # SectionsGarbageCollected : 5
  # ZeroSizedSectionsGarbageCollected : 4
  # NumLinkerScriptRules : 2
  # NumOutputSections : 6
  # NumOrphans : 5
  # NoRuleMatches : 2
  # LinkStats End
  ...

How to interpret the new trampoline naming convention?
--------------------------------------------------------

The Linker trampoline naming convention has been updated to the following format

trampoline_for_<target symbol name>_from_<source input section name>_<Input file
ordinality of the source input section>#<Optional: relocation addend>_<Optional:
Additional trampoline count>

The trampoline naming convention can be broadly classified into 4 cases:

Case 1:
^^^^^^^^

**Format:** trampoline_for_<target symbol name>_from_<source input section name>
_<Input file ordinality of the source input section>

Example:

.. code-block:: bash

  cat > main.c << \!
  int baz() {
    return 0;
  }
  int main ()
  {
    return baz() ;
  }
  !
  cat > script.t << \!
  SECTIONS
  {
    .text           :
    {
      *(.text.main)
    } =0x00c0007f
    . = 0x08000000;
    .text.baz :
    {
      *(.text.baz)
    }
  }
  !

Compile and Link Steps:

.. code-block:: bash

  $ clang -c  main.c -ffunction-sections
  $ ld.eld main.o -T script.t -Map x

Symbols from readelf:

.. code-block:: bash

  $ llvm-readelf -s a.out
  Symbol table '.symtab' contains 9 entries:
     Num:    Value  Size Type    Bind   Vis       Ndx Name
       0: 00000000     0 NOTYPE  LOCAL  DEFAULT   UND
       1: 00000000     0 SECTION LOCAL  DEFAULT     1 .text
       2: 00000000     0 SECTION LOCAL  DEFAULT     3 .comment
       3: 08000000     0 SECTION LOCAL  DEFAULT     2 .text.baz
       4: 00000000     0 FILE    LOCAL  DEFAULT   ABS main.c
       5: 00000014     8 FUNC    LOCAL  DEFAULT     1 trampoline_for_baz_from_.text.main_25
       6: 00000000    20 FUNC    GLOBAL DEFAULT     1 main
       7: 08000000    12 FUNC    GLOBAL DEFAULT     2 baz
       8: 08000011     0 NOTYPE  GLOBAL DEFAULT   ABS __end

**Trampoline Symbol:** trampoline_for_baz_from_.text.main_25 → call baz from main

This is the most vanilla case where the target symbol name, source input section
and input file are used to coin the trampoline name.

Case2:
^^^^^^^

**Format:** trampoline_for_<target symbol name>_from_<source input section name>_
<Input file ordinality of the source input section>_<Additional trampoline count>

Example:

.. code-block:: bash

  cat > main.c << \!
  int far() {
    return 0;
  }
  int callfar() {
    return far() + far();
  }
  int main ()
  {
    return callfar();
  }
  !

  cat > noreuse << \!
  {
      far;
  }
  !

  cat > script.t << \!
  SECTIONS
  {
    .text           :
    {
      *(.text.main)
      *(.text.callfar)
    } =0x00c0007f
    . = 0x08000000;
    .text.far :
    {
      *(.text.far)
    }
  }
  !

**Compile and Link Steps:**

.. code-block:: bash

  $ clang -c  main.c -ffunction-sections
  $ ld.eld main.o -T script.t -Map x -no-reuse-trampolines-file=noreuse

**Symbols from readelf:**

.. code-block:: bash

  $ llvm-readelf -s a.out
  Symbol table '.symtab' contains 11 entries:
     Num:    Value  Size Type    Bind   Vis       Ndx Name
       0: 00000000     0 NOTYPE  LOCAL  DEFAULT   UND
       1: 00000000     0 SECTION LOCAL  DEFAULT     1 .text
       2: 00000000     0 SECTION LOCAL  DEFAULT     3 .comment
       3: 08000000     0 SECTION LOCAL  DEFAULT     2 .text.far
       4: 00000000     0 FILE    LOCAL  DEFAULT   ABS main.c
       5: 00000040     8 FUNC    LOCAL  DEFAULT     1 trampoline_for_far_from_.text.callfar_25
       6: 00000048     8 FUNC    LOCAL  DEFAULT     1 trampoline_for_far_from_.text.callfar_25_1
       7: 00000000    20 FUNC    GLOBAL DEFAULT     1 main
       8: 00000020    32 FUNC    GLOBAL DEFAULT     1 callfar1
       9: 08000000    12 FUNC    GLOBAL DEFAULT     2 far
      10: 08000011     0 NOTYPE  GLOBAL DEFAULT   ABS __end

**Trampoline Symbol:**trampoline_for_far_from_.text.callfar1_25_1 → call far from callfar1

The additional trampoline count is added as a suffix in cases where the reuse of
the existing trampoline is not possible.

.. Note::
  The reuse was not possible because of -no-reuse-trampolines-file=noreuse

Case3:
^^^^^^^

**Format:** trampoline_for_<target symbol name>_from_<source input section name>_#(relocation addend)

**Example:**

.. code-block:: bash

  cat > main.c << \!
  static int baz() {
    return 0;
  }
  int main ()
  {
    return baz();
  }
  !

  cat > script.t << \!
  SECTIONS
  {
    .text           :
    {
      *(.text.main)
    } =0x00c0007f
    . = 0x08000000;
    .text.baz :
    {
      *(.text.baz)
    }
  }
  !

**Compile and Link Steps:**

.. code-block:: bash

  $ clang -c  main.c -ffunction-sections
  $ ld.eld main.o -T script.t -Map x

**Symbols from readelf:**

.. code-block:: bash

  $ llvm-readelf -s a.out
  Symbol table '.symtab' contains 9 entries:
     Num:    Value  Size Type    Bind   Vis       Ndx Name
       0: 00000000     0 NOTYPE  LOCAL  DEFAULT   UND
       1: 00000000     0 SECTION LOCAL  DEFAULT     1 .text
       2: 00000000     0 SECTION LOCAL  DEFAULT     3 .comment
       3: 08000000     0 SECTION LOCAL  DEFAULT     2 .text.baz
       4: 00000000     0 FILE    LOCAL  DEFAULT   ABS main.c
       5: 00000014     8 FUNC    LOCAL  DEFAULT     1 trampoline_for_.text.baz_from_.text.main_25#(0)
       6: 08000000    12 FUNC    LOCAL  DEFAULT     2 baz
       7: 00000000    20 FUNC    GLOBAL DEFAULT     1 main
       8: 08000011     0 NOTYPE  GLOBAL DEFAULT   ABS __end

**Trampoline Symbol:** trampoline_for_.text.bar_from_.text.main_25#(0) →
call to bar from main here #(0) → represents the relocation addend

The relocation addend 0 is added to the trampoline symbol name in case the
trampoline jumps to the section symbol for the symbol

Case4:
^^^^^^^

**Format:** trampoline_for_<target symbol name>_from_<source input section name>
_<Input file ordinality of the source input section>#<relocation addend>
_<Additional trampoline count>

Example:

.. code-block:: bash

  cat > main.c << \!
  static int baz() {
    return 0;
  }
  int main ()
  {
    return baz() + baz();
  }
  !

  cat > noreuse << \!
  {
      baz;
      .text.baz;
  }
  !

  cat > script.t << \!
  SECTIONS
  {
    .text           :
    {
      *(.text.main)
    } =0x00c0007f
    . = 0x08000000;
    .text.baz :
    {
      *(.text.baz)
    }
  }
  !

**Compile and Link Steps:**

.. code-block:: bash

  $ clang -c  main.c -ffunction-sections
  $ ld.eld main.o -T script.t -Map x -no-reuse-trampolines-file=noreuse

**Symbols from readelf:**

.. code-block:: bash

  $ llvm-readelf -s a.out
  Symbol table '.symtab' contains 10 entries:
     Num:    Value  Size Type    Bind   Vis       Ndx Name
       0: 00000000     0 NOTYPE  LOCAL  DEFAULT   UND
       1: 00000000     0 SECTION LOCAL  DEFAULT     1 .text
       2: 00000000     0 SECTION LOCAL  DEFAULT     3 .comment
       3: 08000000     0 SECTION LOCAL  DEFAULT     2 .text.baz
       4: 00000000     0 FILE    LOCAL  DEFAULT   ABS main.c
       5: 00000028     8 FUNC    LOCAL  DEFAULT     1 trampoline_for_.text.baz_from_.text.main_25#(0)
       6: 00000030     8 FUNC    LOCAL  DEFAULT     1 trampoline_for_.text.baz_from_.text.main_25#(0)_1
       7: 08000000    12 FUNC    LOCAL  DEFAULT     2 baz
       8: 00000000    40 FUNC    GLOBAL DEFAULT     1 main
       9: 08000011     0 NOTYPE  GLOBAL DEFAULT   ABS __end

**Trampoline Symbol:** trampoline_for_.text.bar_from_.text.main_25#(0)_1 → 2nd call for bar from main

The duplicate trampoline count was added as a prefix since the symbol name
.text.bar was added to the noreuse list.

**Benefits of the new trampoline naming convention:**

The new format has benefits as follows

* Helps determine/infer a huge amount of info about the target symbol and the
  source sections inherently
* Helps make diff between map files easier since the naming convention is now
  more context-based

Understanding "loadable segments are unsorted by virtual address" warning from llvm-readelf
--------------------------------------------------------------------------------------------

The llvm-readelf can sometimes emit a warning: "loadable segments are unsorted
by virtual address". This occurs in very specific scenarios where below items
are involved

* Dynamic sections
* "-shared" in linker commandline
* The virtual addresses of dynamic sections not in increasing order

Below is a small example to illustrate this:

**Example:**

.. code-block:: bash

  cat > 1.c << \!
  int foo() { return 0; }
  !

  cat > script.t << \!
  PHDRS {
      A PT_LOAD;
      B PT_LOAD;
      DYN PT_DYNAMIC;
  }

  SECTIONS {
        .dynsym (0x900) : { *(.dynsym) } :A
        .dynamic (0x800) : { *(.dynamic) } :B :DYN
  }
  !

  clang -target hexagon -c 1.c -ffunction-sections
  hexagon-link 1.o -T script.t -shared
  llvm-readelf -S -W a.out

Important thing to note in above script is the non-increasing order of virtual
addresses of the dynamic sections

When the *"llvm-readelf"* in above script runs, a specific code branch for
dynamic images in ELF.cpp emits this warning

**Output:**

.. code-block:: bash

  llvm-readelf: warning: 'a.out': loadable segments are unsorted by virtual address

This warning in its current form is rather vague and very general as it doesn't
inform the user about the specific dynamic builds this warning is meant for.

Currently, there is **no** option available to suppress this warning.

Linker overlap checks
----------------------
Linker has been improved to detect overlaps in the image layout. Linker does this by default.
The following overlaps are detected by the linker.
* Virtual address overlaps
* Physical address overlaps
* File offset overlaps

If the overlaps are known and you want to turn this behavior OFF, you can use
*--no-check-sections* flag.

Input File to Linker
=====================

The linker takes the following kinds of input files as input :-

* Object files
* Shared libraries
* Linker scripts
* Multiple command line options
* Other kinds of input files such as
    * Extern list
    * dynamic list
    * version scripts
    * linker plugin configuration files

Most recently the linker also was modified to support taking fully built static
executables as part of the link step.

ELF executable as input files to the linker
---------------------------------------------

Linker allows a fully built static executable as input to the linker.

The LLVM community linker does not support this option. GNU linker used to support
but now does not as per the below bug

https://sourceware.org/bugzilla/show_bug.cgi?id=26223

ELD allows fully built static executables as input to the linker as per the below example.

.. code-block:: bash

  cat > script.t << \!
  SECTIONS {
    .text : {
      *(.text)
    }
    . = 0x2000;
    anotherelf : {
      anotherexec.elf(.bar)
    }
  }
  !

  cat > foo.c << \!
  extern int bar();
  int foo() { return bar(); }
  !

  cat > bar.c << \!
  __attribute__((section(".bar"))) int bar() { return 0; }
  !

  $clang -O2 -c bar.c -fno-exceptions -fno-asynchronous-unwind-tables
  $clang -c foo.c -fno-exceptions -fno-asynchronous-unwind-tables
  $link bar.o -o anotherexec.elf -Ttext=0x2000
  $link foo.o anotherexec.elf -T script.t

.. warning::
  This is strongly discouraged by the community and got accidentally removed in the GNU linker.

  It is upto the user to make sure that the linker script places the executable
  code previously linked at the same address.

  Linking dynamic executables is not possible with this functionality and will not be supported

.. note::
  If you really need to link with executables, we recommend to use the --just-symbols option

Linker Script General Questions
=================================

Debugging section placements
------------------------------

Linker Script Rules
----------------------

Input sections can be stored in files or archives. They are specified in the SECTIONS
command with the following syntax:

*[path][archive:][file](section...)*

The standard wildcard characters (\*, ?, etc.) can be used anywhere in an input section
specification to do the following:

* Specify multiple paths, archives, or files where sections will be searched for
* Specify multiple sections as input sections

.. list-table:: Title

   :widths: 25 25 50
   :header-rows: 1

   * - Specification
     - Description
   * - dir/subdir/init.lib:init.o(.text.*)
     - Specify one or more .text. sections from a specific object file
       (init.o) in a specific archive (init.lib)
   * - dir/subdir/init.lib:(.text.*)
     - 	Specify one or more .text. sections from any object file in the
        specified archive (init.lib)
   * - dir/subdir/*:(.text.*)
     - Specify one or more .text. sections from any archive in the
       specified directory (dir/subdir)
   * - \*(.text.*)
     - Specify one or more .text. sections from any archive or object
       file in the entire file system

Fill Values
--------------

Output Section Fill
^^^^^^^^^^^^^^^^^^^^

You can set the fill pattern for an entire section by using ‘=fillexp’.
fillexp is an expression.

Any otherwise unspecified regions of memory within the output section
(for example, gaps left due to the required alignment of input sections)
will be filled with the value, repeated as necessary.

**In all cases, the number specified by the fillexp is big-endian.**

Here is a simple example:

.. code-block:: bash

  SECTIONS { .text : { *(.text) } =0x90909090 }

I am getting an error, no linker script rule for ".bss"
--------------------------------------------------------

The way that you would investigate this is to figure out if there is an actual
.bss section that is present in the input files, not selected by any linker
script rule. Emit the Map file and look at the section .bss and see what kind
of sections are present.

* If it shows that a input file with a particular section is being listed and
  not present in any of the patterns, you will likely need to go and add a rule
  such as
    * \*(.bss) so that you are sure of where you want to place it.
* If all the input file and sections are selected, you should go and look at if
  the section corresponds to a common symbol
    * You will most probably be missing a rule as below
        * \*(COMMON)
* Try to do the link step again with any of the above changes and your error
  should have gone.

I am getting an error, no linker script rule for ".data.bar"
-------------------------------------------------------------

This is most often a problem if there is a compiler flag that has changed in
your environment or a change of tools. You would need to fix the linker script
and add a rule as below.

* \*(.data.bar)

Is there a way to find all used/unused rules in the Linker script ?
---------------------------------------------------------------------

You can use a simple grep pattern to check for used/unused rules in the Map file.

UnUsed Rules
^^^^^^^^^^^^^^

.. code-block:: bash

  grep "Rule.*\[0:" link.map | grep -v Implicit

Used Rules
^^^^^^^^^^^^

.. code-block:: bash

  grep "Rule.*\[[1-9]*:" link.map  | grep -v Implicit

How do I exclude sections from object files/archive files
------------------------------------------------------------

Excluding sections from object files
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The example illustrates users trying to exclude all text sections from being
placed in section .text1 and when the input file is an object file.

.. code-block:: bash

  cat > a.c << \!
  int foo() { return 0; }
  int bar() { return 0; }
  !


  cat > script.t << \!
  SECTIONS {
    .text1 : {
      *(EXCLUDE_FILE(a.o) .text.*)
    }
    .text2 : {
      *(.text*)
    }
  }
  !

  clang -target hexagon -ffunction-sections -fdata-sections -c a.c -G0
  rm -f lib1.a
  hexagon-ar cr lib1.a a.o
  hexagon-link -T script.t a.o -Map x

Exclude all patterns of text section from one file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The example illustrates an user trying to exclude all text sections from being
placed in section .text1.

In this example, all code from lib1.a/a.o is not emitted to the the .text1 section

.. code-block:: bash

  cat > a.c << \!
  int foo() { return 0; }
  int bar() { return 0; }
  !


  cat > script.t << \!
  SECTIONS {
    .text1 : {
      *lib1.a:(EXCLUDE_FILE(a.o) .text.*)
    }
    .text2 : {
      *(.text*)
    }
  }
  !

  clang -target hexagon -ffunction-sections -c a.c
  rm -f lib1.a
  hexagon-ar cr lib1.a a.o
  hexagon-link -T script.t --whole-archive lib1.a

Excluding all patterns of text section from more than one file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The example illustrates an user trying to exclude all text sections from being
placed in section .text1.

In this example, all code from lib1.a/a.o and lib1.a/b.o is not emitted to the
.text1 section

.. code-block:: bash

  cat > a.c << \!
  int foo() { return 0; }
  int bar() { return 0; }
  !

  cat > b.c << \!
  int baz() { return 0; }
  !


  cat > script.t << \!
  SECTIONS {
    .text1 : {
      *lib1.a:(EXCLUDE_FILE(a.o b.o) .text.*)
    }
    .text2 : {
      *(.text*)
    }
  }
  !

  clang -target hexagon -ffunction-sections -fdata-sections -c a.c -G0
  clang -target hexagon -ffunction-sections -fdata-sections -c b.c -G0
  rm -f lib1.a
  hexagon-ar cr lib1.a a.o b.o
  hexagon-link -T script.t --whole-archive lib1.a -Map x

Specifying multiple exclude patterns
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can specify more than one EXCLUDE_FILE pattern in a linker script rule.

This example below illustrates a way that the user can specify multiple
EXCLUDE_FILE linker script commands.

.. code-block:: bash

  cat > a.c << \!
  int foo() { return 0; }
  int bar() { return 0; }
  !

  cat > b.c << \!
  int baz() { return 0; }
  !


  cat > script.t << \!
  SECTIONS {
    .text1 : {
      *lib1.a:(EXCLUDE_FILE(a.o) .text.foo EXCLUDE_FILE(a.o) .text.bar EXCLUDE_FILE(b.o) .text.baz)
    }
    .text2 : {
      *(.text*)
    }
  }
  !

  clang -target hexagon -ffunction-sections -fdata-sections -c a.c -G0
  clang -target hexagon -ffunction-sections -fdata-sections -c b.c -G0
  rm -f lib1.a
  hexagon-ar cr lib1.a a.o b.o
  hexagon-link -T script.t --whole-archive lib1.a -Map x

Common mistakes with exclude files
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Specifying more than one pattern

.. warning::
  A common mistake users do is when they specify more than one pattern with one
  occurrence of an EXCLUDE_FILE rule.
  Only the first pattern following the EXCLUDE_FILE rule is used for matching

In the above example if the user did the following

.. code-block:: bash

  SECTIONS {
    .text1 : {
      *lib1.a:(EXCLUDE_FILE(a.o) .text.foo .text.bar EXCLUDE_FILE(b.o) .text.baz)
    }
    .text2 : {
      *(.text*)
    }
  }

.. Note::
  The function specified by .text.bar would be placed in .text1 since .text.bar
  is following the pattern .text.foo.

Not specifying all the files in the EXCLUDE_FILE pattern
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When the file pattern does not match the filename specified in EXCLUDE_FILE,
pattern following the EXCLUDE_FILE is used to select input sections that do not
originate from that file.

Below example illustrates that. In this example .text.baz from file b.o is
selected by the rule, which results in .text1 housing .text.baz.

.. code-block:: bash

  cat > a.c << \!
  int foo() { return 0; }
  int bar() { return 0; }
  !

  cat > b.c << \!
  int baz() { return 0; }
  !


  cat > script.t << \!
  SECTIONS {
    .text1 : {
      *lib1.a:(EXCLUDE_FILE(a.o) .text.*)
    }
    .text2 : {
      *(.text*)
    }
  }
  !

  clang -target hexagon -ffunction-sections -fdata-sections -c a.c -G0
  clang -target hexagon -ffunction-sections -fdata-sections -c b.c -G0
  rm -f lib1.a
  hexagon-ar cr lib1.a a.o b.o
  hexagon-link -T script.t --whole-archive lib1.a -Map x


Specifying EXCLUDE_FILE directive that applies to multiple section patterns
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

We can specify EXCLUDE_FILE  directive that applies to multiple section patterns
by placing EXCLUDE_FILE before the corresponding <file-patterns>(<section-patterns>).

For Example:

.. code-block:: bash

  #!/usr/bin/env bash

  cat >1.c <<\EOF
  int foo() { return 1; }
  int baz() { return 3; }
  EOF

  cat >2.c <<\EOF
  int bar() { return 3; }
  EOF

  cat >script.t <<\EOF
  SECTIONS {
    FOO_BAZ : { EXCLUDE_FILE(2.o) *(.text.foo .text.baz .text.bar) }
    BAR : { *(*.text*) }
  }
  EOF

  clang -target hexagon -o 1.o 1.c -c -ffunction-sections
  clang -target hexagon -o 2.o 2.c -c -ffunction-sections
  hexagon-link -o a.out 1.o 2.o -T script.t

In the above example, EXCLUDE_FILE(2.o) applies to all the section patterns
present in the sub-rule: \*(.text.foo .text.baz .text.bar)

NOCROSSREFS
^^^^^^^^^^^^

The NOCROSSREFS command takes a list of output section names. If the linker
detects any cross references between the sections, it reports an error and
returns a non-zero exit status. Note that the NOCROSSREFS command uses output
section names, not input section names.

.. code-block:: bash

  cat > 1.c << \!
  extern int bar();
  int foo() { return bar(); }
  !

  cat > 2.c << \!
  int bar() { return 0; }
  !

  cat > script.t << \!
  NOCROSSREFS(.foo .bar)
  SECTIONS {
    .foo : { *(.text.foo) }
    .bar : { *(.text.bar) }
  }
  !

  clang -c 1.c 2.c -ffunction-sections
  ld.eld 1.o 2.o -T script.t

The above linker script usage of **NOCROSSREFS** produces an error because content
in output section foo is calling into bar.

.. role:: red

  Error: 1.o:(.text.foo:0x8): prohibited cross reference from .foo to `bar'(2.o) in .bar

How to discard an input section?
----------------------------------

Input sections can be explicitly discarded from the output image by assigning
the input section to a special output section named '/DISCARD/'. It can also
discard sections marked with the ELF flag SHF_GNU_RETAIN which would otherwise
have been saved from linker garbage collection.

For Example:

.. code-block:: bash

  cat >1.c <<\EOF
  __attribute__((retain)) int foo() { return 0; }
  int bar() { return 0; }
  int main() { return 0; }
  EOF

  cat >1.linker.script <<\EOF
  SECTIONS {
    /DISCARD/ : { *(.text.foo) }
  }
  EOF

  clang -target hexagon -o 1.o -c 1.c -ffunction-sections
  hexagon-link -o 1.elf 1.o --gc-sections --print-gc-sections -e main
  hexagon-link -o 1.with_script.elf 1.o -T 1.linker.script --gc-sections --print-gc-sections -e main --trace-section .text.foo

**hexagon-link -o 1.elf 1.o --gc-sections --print-gc-sections -e main**

Running the above command gives the following output:

Trace: GC : 1.o[.text]
| Trace: GC : 1.o[.text.bar]

Please note that '.text.foo' is preserved even though there are no references
to the function foo from the entry point. It is because '.text.foo' is marked
with the ELF flag SHF_GNU_RETAIN using '__attribute__((retain))'.

The above command can be run with the verbose option to see the
'retain section...' diagnostic for '.text.foo':

Verbose: Retaining section .text.foo from file 1.o

'.text.foo' can be discarded by assigning it to /DISCARD/ special output section.

hexagon-link -o 1.with_script.elf 1.o -T 1.linker.script --gc-sections --print-gc-sections -e main --trace-section .text.foo

Running the above command gives the following output:

Note: Input Section : .text.foo InputFile : 1.o Alignment : 0x10 Size : 0xc Flags : SHF_ALLOC|SHF_EXECINSTR
| Note: Input Section : .text.foo Symbol : foo
| Note: Section : .text.foo is being discarded. Section originated from input : 1.o
| Trace: GC : 1.o[.text]
| Trace: GC : 1.o[.text.bar]

We can see in the 'Note' diagnostic that '.text.foo' has been discarded.

Marking a loadable output section as non loadable
---------------------------------------------------

Linker now supports loadable sections to be listed after non loadable sections,
so the linker is able to place them in a loadable segment.

There have been various assumptions in the past that customers relied on their linker
scripts, about linker not supporting loadable sections after non loadable sections.

If these assumptions are still valid for your use case(s) :

* Make sure to list the ouput section to be of type INFO
  * Example : OutputSection **(INFO)**
  * This will mark the output section non loadable
* Always inspect the output of your section to segment mapping before loading the image
  * llvm-readelf -l -W *outputfile*

**If you have been waiting for this feature, remove the virtual address assigned
to the section.**

Most users have these output sections listed in the linker script to use a
virtual address of 0. You will need to remove it.

Always inspect the output image to check for any discrepancies.

What does KEEP mean for input section descriptions in /DISCARD/? (Added in HEX 8.8)
------------------------------------------------------------------------------------

KEEP marks all the input sections matched by an input section description as an
entry section. Entry sections are not subject to garbage-collection and are used
to calculate live-edges for garbage-collection. However, entry sections can still
be discarded. Therefore, behavior of KEEP is the same for input section description
of all output sections, including the /DISCARD/ output section. Important thing
to remember is when KEEP is used for input section descriptions in the/DISCARD/
output section then the matched input sections are treated as entry sections for
live-edge computation but are still discarded nonetheless.

Example:

.. code-block:: bash

  #!/usr/bin/env bash

  cat >1.c <<\EOF
  int bar() {
    return 1;
  }

  int foo() {
    return bar();
  }

  int baz() {
    return 3;
  }

  int abc() {
    return 5;
  }

  int main() {
    return baz();
  }
  EOF

  cat >script.t <<\EOF
  SECTIONS {
    /DISCARD/ : { KEEP(*(*.foo)) }
    .text : { *(.text.*) }
  }
  EOF

  clang -target hexagon -o 1.o 1.c -c -ffunction-sections -fdata-sections
  hexagon-link -o 1.elf 1.o -T script.t --gc-sections --print-gc-sections -e main -Map 1.map.tx

In this example, bar  is not garbage-collected because it is reachable by foo.
foo  is an entry section due to KEEP specifier even though it is discarded. Map
file can be used to confirm the behavior.

How to build executables by linking symbols from an another ELF file?
-----------------------------------------------------------------------

Users can build executables by linking symbols from another ELF file by using
the SYMDEF feature.

ELD mimics the functionality available in the ARM linker for this.

This method is also used by ARM baremetal builds for linking against symbols
that exist in another image.

SymDef file
^^^^^^^^^^^^

A symdef file is a file that consists of all global symbols that can be used in
future link steps.

The linker produces a symdefs file during a successful final link stage. It is
not produced for partial linking or for unsuccessful final linking.

The symdef file is produced by using the linker symdef option.

Example:

.. code-block:: bash

  cat >1.c << \!
  int foo() {return bar(); }
  !

  cat > 2.c << \!
  int boo() { return baz(); }
  !

  cat > 3.c << \!
  int bar() { return 0; }
  int baz() { return 0; }
  !

  cat > script.t << \!
  SECTIONS {
    .foo : { *(.text.*) }
  }
  !

  cat > image.t << \!
  SECTIONS {
    . = 0xF0000000;
    .text : { *(.text*) }
  }
  !

  $CLANG -c 1.c -ffunction-sections
  $CLANG -c 2.c -ffunction-sections
  $CLANG -c 3.c -ffunction-sections
  $LINK -o otherimage.elf 3.o --symdef-file r.symdef -T image.t
  # otherimage.elf should be preserved for the below link command to work
  $LINK 1.o 2.o r.symdef -T script.t -o out.elf

The symdef file gets produced that contains the address of bar and baz in the image.

.. code-block:: bash

  #<SYMDEFS>#
  #DO NOT EDIT#
  0xf0000000      FUNC    bar
  0xf0000010      FUNC    baz

Later the file is used in the secondary link to generate the complete binary.

How to specify Load Memory Address (LMA) of a section?
--------------------------------------------------------

Load Memory Address (LMA) of a section can be specified using the 'AT' command.
Please note that it is different from the virtual memory address.

.. code-block:: bash

  FOO 0x1000 : AT(0x4000) {
     *(*foo*)
   }

In the above example, '0x1000' is the virtual memory address (VMA) of the output
section 'FOO', and '0x4000' is the load memory address (lma).

A concrete example:

.. code-block:: bash

  #!/usr/bin/env bash

  cat >1.c <<\EOF
  int foo() { return 1; }
  int bar() { return 3; }
  EOF

  cat >script.t <<\EOF
  SECTIONS {
    FOO 0x1000 : AT(0x4000) {
      *(*foo*)
    }

    BAR 0x2000 : AT(0x6000) {
      *(*bar*)
    }
  }
  EOF

  cat >script.without_at.t <<\EOF
  SECTIONS {
    FOO 0x1000 : { *(*foo*) }
    BAR 0x2000 : { *(*bar*) }
  }
  EOF

  CCs=("clang -target hexagon" clang-15 clang-15)
  LDs=(hexagon-link ld.bfd ld.lld)
  SFs=(eld bfd lld)

  clang -target hexagon -o 1.o 1.c -c -ffunction-sections
  hexagon-link -o 1.elf 1.o -T script.t
  hexagon-llvm-objdump -h 1.elf

  # Output:
  # 1.eld.elf:     file format elf32-hexagon
  #
  # Sections:
  # Idx Name          Size     VMA      LMA      Type
  #  0               00000000 00000000 00000000
  #  1 FOO           0000000c 00001000 00004000 TEXT
  #  2 BAR           0000000c 00002000 00006000 TEXT
  #  3 .comment      00000106 00000000 00000000
  #  4 .shstrtab     0000002c 00000000 00000000
  #  5 .symtab       00000080 00000000 00000000
  #  6 .strtab       00000024 00000000 00000000

.. code-block:: bash

  Please note the VMA and LMA of output sections FOO and BAR:

  FOO           0000000c 00001000 00004000
  BAR           0000000c 00002000 00006000

Image layout using LMA
------------------------

When sections are placed in a segment, the LMA address of the section is
calculated as the sum of LMA address of the segment  and virtual address offset
of the section from the beginning of the segment.

User can query the LMA address of the section using the linker script keyword
LOADADDR.

.. code-block:: bash

  cat > 1.c << \!
  int bss[100] = { 0 };
  int data[100] = { 0 };
  int foo() { return 0; }
  !

  cat > script.t << \!
  PHDRS {
    A PT_LOAD;
  }

  SECTIONS
    .foo : AT(0x1000) { *(.text.foo) }  :A
    .bar : { *(.bss.bss*) }  :A
    .baz : { *(.bss.data*) }  :A
    .nothing : { } :A
    load_addr_baz = LOADADDR(.nothing);
  }
  !

  $clang -c 1.c -ffunction-sections -fdata-sections -G0 -fno-asynchronous-unwind-tables
  $link 1.o -T script.t -Map x

With the above example you can see that the linker sets the value of load_addr_baz
to the load address of the section .nothing.

For Hexagon architecture, you can see that the load address thats calculated
accounts to be 0x1330.

.. code-block:: bash

  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD           0x001000 0x00000000 0x00001000 0x0000c 0x00330 RWE 0x1000

This change in behavior is observed from linkers on release

* Hexagon 8.9 and above
* RISC-V 18.0 release

If you have a linker script that assumes the behavior of LOADADDR, you might want
to fix that with the recent change to behavior.

What are BYTE/SHORT/LONG/QUAD/SQUAD linker script commands?
-------------------------------------------------------------

These commands allow including explicit bytes of data in an output section.
Each of these commands is followed by an expression in parentheses providing the
value to store. The value is stored at the current value of the location counter

The BYTE, SHORT, LONG, and QUAD commands store one, two, four, and eight bytes (respectively).
After storing the bytes, the location counter is incremented by the number of bytes stored.

An example demonstrating these commands:

.. code-block:: bash

  #!/usr/bin/env bash

  cat >1.c <<\EOF
  int foo() { return 1; }
  EOF

  cat >script.t <<\EOF
  SECTIONS {
    FIVE: { BYTE(0x5) }
    SIXTEEN : { LONG(0x10) }
  }
  EOF

  clang -target hexagon -o 1.o 1.c -c
  hexagon-link -o 1.elf 1.o -T script.t
  hexagon-readelf -x 'FIVE' 1.elf
  hexagon-readelf -x 'SIXTEEN' 1.elf

What is the order of linker script assignment evaluations?
-----------------------------------------------------------

Linker script contains assignment expressions. Assignment expressions can be
categorized into 3 distinct categories: before-sections, in-sections and after-sections.
We will use the term outside-sections to collectively refer before-sections and
after-sections categories. When an assignment expression gets evaluated depends
on the assignment expression category.

First, all the assignment expressions outside the SECTIONS commands are evaluated
and then the assignment expressions within the SECTIONS command are evaluated.
There are few catches, which we will soon explore.

Let's look at some concrete examples to understand assignment evaluation order behavior.

Basic
^^^^^^

.. code-block:: bash

  #!/usr/bin/env bash

  cat >script.t <<\EOF
  bar = 0x3;
  SECTIONS {
    baz = bar;
  }
  EOF

  touch 1.c
  clang -target hexagon -o 1.o 1.c -c
  hexagon-link -o 1.out 1.o -T script.t
  hexagon-readelf -s 1.out
  # Output:
  # Symbol table '.symtab' contains 7 entries:
  #   Num:    Value  Size Type    Bind   Vis       Ndx Name
  #     // ...
  #     // ...
  #     5: 00000003     0 NOTYPE  GLOBAL DEFAULT   ABS bar
  #     6: 00000003     0 NOTYPE  GLOBAL DEFAULT   ABS baz

Both bar and baz have the value 0x3 as expected.

Use an after-sections variable to initialize an in-sections variable
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

  #!/usr/bin/env bash

  cat >script.t <<\EOF
  SECTIONS {
    baz = bar;
  }
  bar = 0x3;
  EOF

  touch 1.c
  clang -target hexagon -o 1.o 1.c -c
  hexagon-link -o 1.out 1.o -T script.t
  hexagon-readelf -s 1.out
  # Output:
  # Symbol table '.symtab' contains 7 entries:
  #   Num:    Value  Size Type    Bind   Vis       Ndx Name
  #     // ...
  #     // ...
  #     5: 00000003     0 NOTYPE  GLOBAL DEFAULT   ABS bar
  #     6: 00000003     0 NOTYPE  GLOBAL DEFAULT   ABS baz

Both bar and baz have the value 0x3 even though at first glance it seems that
bar is defined later than baz.

That's not true. Remember that all outside-sections assignments are evaluated
before in-sections assignments. Hence, bar is evaluated before baz.

Use a variable, that is defined in both before-sections and after-sections assignment, to initialize an in-sections variable
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

  #!/usr/bin/env bash

  cat >script.t <<\EOF
  bar = 0x3;
  SECTIONS {
    baz = bar;
  }
  bar = 0x5;
  EOF

  touch 1.c
  clang -target hexagon -o 1.o 1.c -c
  hexagon-link -o 1.out 1.o -T script.t
  hexagon-readelf -s 1.out
  # Output:
  # Symbol table '.symtab' contains 7 entries:
  #   Num:    Value  Size Type    Bind   Vis       Ndx Name
  #     // ...
  #     // ...
  #     5: 00000005     0 NOTYPE  GLOBAL DEFAULT   ABS bar
  #     6: 00000005     0 NOTYPE  GLOBAL DEFAULT   ABS baz

Now, that's a little interesting. If the assignments were evaluated in the
natural-order then we would have seen baz value as 0x3 and bar value as 0x5.
However, this is not what we have observed.

The reason is that outside-sections assignments are evaluated before in-sections
assignments. Hence, the order of evaluations is:

1. bar = 0x3
2. bar = 0x5
3. baz = bar

**So remember that the golden rule is outside-sections assignments are evaluated
before in-sections assignments.**

Using a defsym symbol in linker script assignment expression
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

  #!/usr/bin/env bash

  cat >script.t <<\EOF
  SECTIONS {
    baz = bar;
  }
  EOF

  touch 1.c
  clang -target hexagon -o 1.o 1.c -c
  hexagon-link -o 1.out 1.o -T script.t --defsym bar=0x5
  hexagon-readelf -s 1.out
  # Output:
  # Symbol table '.symtab' contains 7 entries:
  #   Num:    Value  Size Type    Bind   Vis       Ndx Name
  #     // ...
  #     // ...
  #     5: 00000005     0 NOTYPE  GLOBAL DEFAULT   ABS bar
  #     6: 00000005     0 NOTYPE  GLOBAL DEFAULT   ABS baz

*defsym*  symbols are treated as outside-sections symbols, and hence they are
always evaluated before in-sections symbols. This explains the readelf output
that we see.

What linker script changes are required for supporting thread-local storage (TLS)?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If any of the linker inputs use thread-local storage (TLS), then some linker script
changes are required to correctly support thread-local storage functionality.

To properly understand these linker script changes, it is helpful to understand
the TLS functionality and how TLS is allocated and initialized.

Thread-local storage functionality makes a variable local to each thread.
This means that each thread has its own copy of the variable. This is unlike
ordinary global/static variables that are shared across all threads.
The runtime library allocates thread-local storage for each thread and
it initializes the thread-local storage to the region pointed by the
:code:`PT_TLS` segment. Among other things, :code:`PT_TLS` segment
describes to the runtime library where to find the initial contents
of the thread-local storage and the alignment requirements.

The runtime library needs :code:`PT_TLS` segment for properly initializing
TLS region for each block. Thus, the linker needs to emit :code:`PT_TLS`
segment for the images that are utilizing TLS functionality. The linker
script changes are required for instructing the linker how to properly
emit :code:`PT_TLS` segment. In particular, we need to add a :code:`PT_TLS`
program header and put the TLS sections (:code:`.tdata` and :code:`.tbss`)
into both the :code:`PT_TLS` and a :code:`PT_LOAD` section. For example:

.. code-block:: bash

   PHDRS {
     ...
     DATA PT_LOAD;
     TLS PT_TLS;
     ...
   }

   SECTIONS {
     ...
     .tbss : { *(*.tbss) } :DATA :TLS
     .tdata : { *(*.tdata) } :DATA :TLS
     .data : { *(*.data) } :DATA // It is important to specify :DATA here
                                 // otherwise ':DATA :TLS' will be assumed
     ...
   }

Why am I getting the warning 'Space between archive:member file pattern is deprecated'
-----------------------------------------------------------------------------------------

Linker script allows to specify archive members in input section descriptions
using the `<archive-pattern>:<member-pattern>` syntax. For example in the below
linker script `SECTIONS` command snippet, `foo_text` output section contains the
`.text*` sections from the `foo.o` member of `libfoobar.a` archive.

.. code-block:: bash

   SECTIONS {
     foo_text : { libfoobar.a:foo.o(.text*) }
   }


Please note that there should be no space between the `<archive-pattern>:`
and the `<member-pattern>`. Previously, the linker accepted the space
between the two patterns for convenience. This behavior is now
deprecated and will be removed in the future.

The below linker script snippet demonstrates some of the cases where
the warning will be emitted and why.

.. code-block:: bash

   cat > 1.c << \!
   int data = 10;
   !

   cat > script.t << \!
   SECTIONS {
     .data : {
        /* The warning will be displayed because there is a space between
           '*lib1.a:' and '1.o' */
       *lib1.a: 1.o(.*data*)
        /* No warning. When member-pattern is missing, all members of the
           matched archives are matched. */
       /tmp/lib1.a*: (.*data*)
       *lib1.a: (.*data*)
       /* The warning will be displayed because there is a space between
          '*lib1.a:' and '*' */
       *lib1.a: *(.*data*)
       /* No warning. '*lib1.a:' and '*(.data*)' are considered as separate
          input section descriptions. */
       *lib1.a:
       *(.data*)
     }
   }
   !

  clang -c   -c 1.c -g
  llvm-ar cr lib1.a 1.o
  ld.eld --whole-archive lib1.a -T script.t -Map x -Wlinker-script

Linker Script PHDRS
====================

When should I use PHDRS command
--------------------------------

PHDRS command should be used only when you want to control how segments are
created by the linker.

If you don't list a segment in the linker script when using PHDR's, the segment
will not be created by the linker.

What this means, is that the user needs to explicit specify a segment in the
linker script for any specific needs of the loader or the image at runtime.

My output file is big!
-----------------------

An output file may become huge or big depending on how you have created the
linker script file. Most use cases

* Will show that the user uses the linker script directive PHDRS.
* The user has specified a virtual address hardcoded in the linker script

For figuring out the issue, its always useful to look at the Map file.

Look for the section where the Map file shows a bigger offset than expected.
Most often you will need to change the linker script to remove the hardcoded
virtual address for the output section.

Loadable section not in any load segment
------------------------------------------

This error is experienced when the user has a loadable section, but the user has
not assigned a PT_LOAD segment for it. You might want to look at the list of
segments and the segment assignments for each output section.

Segments need to be mentioned contiguously in the linker script
----------------------------------------------------------------

Segments need to be contiguous in the linker script, or the virtual address
assigned to the segment need to be correctly assigned based on where the previous
segment ended for that particular segment.

This is because file size and memory size of the segment needs to be calculated
correctly, and the segment needs to be declared contiguously.

The file size / memory size is calculated by the difference between the first output
section in the segmet and the last output section in the segment.

If there are segments declared in between, they will create these kinds of
duplicate sections.

Below is an example that has duplicate sections in the segment list.

.. code-block:: bash

  cat > 1.c << \!
  int bss[10000] = { 0 };
  int data = 100;
  int foo() { return 0; }
  !cat > script.t << \!
  PHDRS {
    A PT_LOAD;
    B PT_LOAD;
  }
  SECTIONS {
    .text : { *(.text*) } :A
    .bss : { *(.bss*) } :B
    .data : { *(.data*) } :A
  }
  !
  clang -target hexagon -c 1.c -ffunction-sections -fdata-sections -G0
  hexagon-link 1.o -T script.t

**readelf output:**

.. code-block:: bash

  $ readelf -l -W a.outElf file type is EXEC (Executable file)
  Entry point 0x0
  There are 2 program headers, starting at offset 52Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x001000 0x00000000 0x00000000 0x09c54 0x09c54 RWE 0x1000
    LOAD           0x00b010 0x00000010 0x00000010 0x00000 0x09c40 RW  0x1000 Section to Segment mapping:
    Segment Sections...
     00     .text .bss .data
     01     .bss


Getting file header and program header to be loaded
-----------------------------------------------------

Often times, it may be required by the runtime loader to inspect the file header
and the program header.

You can use FILEHDRS and PHDRS as part of using PHDRS command.

.. code-block:: bash

  cat > 1.c << \!
  int data = 20;
  int foo() { return 0; }
  !

  cat > script.t << \!
  PHDRS {
    text PT_LOAD FILEHDR PHDRS;
    data PT_LOAD;
  }

  SECTIONS {
    . = SIZEOF_HEADERS;
    .foo : { *(.text.foo) } :text
    .data : { *(.data.data) } :data
  }
  !

  $CLANG -c 1.c -ffunction-sections -fdata-sections -G0
  $LD 1.o -T script.t

To verify that the linker did the right thing, you can use readelf output to
figure out.

**readelf output**

.. code-block:: bash

  Elf file type is EXEC (Executable file)
  Entry point 0x0
  There are 2 program headers, starting at offset 52

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x000000 0x00000000 0x00000000 0x0008c 0x0008c R E 0x1000
    LOAD           0x00008c 0x0000008c 0x0000008c 0x00004 0x00004 RW  0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .foo
     01     .data

If you see the above, the first load segment starts from offset 0, which means
the file header gets loaded when the executable runs.

Often times this may be required for building shared libraries, as the dynamic
loader needs the file header and program header for relocating it.

Using AT along with loading file headers and program headers
-------------------------------------------------------------

Users need to be careful when including file headers and program headers to be
loaded using PHDRS linker script command, and also set a physical address for
the section.

Since the file headers and program headers are loaded, linker needs to account
for the physical address when creating the segments.

Below is an example that shows an image built with a linker script and uses AT
linker script directive.

**PHDRS with AT Command**

.. code-block:: bash

  cat > 1.c << \!
  int foo() { return 0; }
  !

  cat > script.t << \!
  PHDRS {
    A PT_LOAD FILEHDR PHDRS;
  }

  SECTIONS {
    . = 0xd820000;
    . = . + SIZEOF_HEADERS;
    .text : AT(0xd820000) { *(.text*) } :A
  }
  !

  clang -c -target aarch64 -c 1.c  -ffunction-sections -fno-asynchronous-unwind-tables
  ld.eld -march aarch64 1.o -T script.t

Here the linker script shows that the text section has a physical address of
0xd820000, but the user also has said that the FILEHDR and PHDRS to be loaded as
part of the load segment.

Linker automatically moves the physical address of the segment to satisfy that
the text section has a physical address that can be met as per user needs.

To fix the problem and account for the adjustment of physical addresses, the
user needs to make sure that the physical address assigned accounts for the
SIZEOF_HEADERS increase.

Recommendation : The user also can drop the usage of AT which seems to be
unnecessary to simplify the build step.

**Linker Script Fix**

.. code-block:: bash

  cat > script.t << \!
  PHDRS {
    A PT_LOAD FILEHDR PHDRS;
  }

  SECTIONS {
    . = 0xd820000;
    . = . + SIZEOF_HEADERS;
    .text : AT(0xd820080) { *(.text*) } :A
  }
  !

Using PHDR flags
-----------------

Developers can use PHDR flags to convey information from the elf image built to
the loader.

The ELF specification provides a convenient way to record this information using
phdr flags.

.. code-block:: bash

  #define PF_MASKOS       0x0ff00000      /* OS-specific */
  #define PF_MASKPROC     0xf0000000      /* Processor-specific */

All bits included in the *PF_MASKOS* mask are reserved for operating system-specific
semantics.

All bits included in the PF_MASKPROC mask are reserved for processor-specific
semantics. If meanings are specified, the processor supplement explains them.

An example of setting this flag is as below :-

.. code-block:: bash

  PHDRS {
    A PT_LOAD FLAGS (0x03000000);
  }

  SECTIONS {
  .text : { *(.text.*) } :A
  }

Using the above linker script, you have a segment "A" that contains loadable text
but with a OS specific property recorded in the program header.

Loading only the program header
--------------------------------

In the above example, we illustrated how an user can load the file header and
the program header.

Sometimes it may be required that the user does not want file header but only
program header to be loaded.

Below example illustrates the behavior:

Example:

.. code-block:: bash

  cat > 1.c << \!
  int data = 20;
  int foo() { return 0; }
  !

  cat > script.t << \!
  PHDRS {
    text PT_LOAD PHDRS;
    data PT_LOAD;
  }

  SECTIONS {
    . = SIZEOF_HEADERS;
    .foo : { *(.text.foo) } :text
    .data : { *(.data.data) } :data
  }
  !

  $CC -c 1.c -ffunction-sections -fdata-sections -G0
  $LD 1.o -T script.t

You can inspect the resulting executable to see if program headers have been
loaded.

**readelf output**

.. code-block:: bash

  $ readelf -l -W a.out

  Elf file type is EXEC (Executable file)
  Entry point 0x0
  There are 2 program headers, starting at offset 52

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x000034 0x00000034 0x00000034 0x00058 0x00058 R E 0x1000
    LOAD           0x00008c 0x0000008c 0x0000008c 0x00004 0x00004 RW  0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .foo
     01     .data

If you see the above, the first load segment starts from offset 34, which means
the program header gets loaded when the executable runs.

String Merging
================

Merging order
---------------

On 8.7, the linker deduplicates strings according to link order. On >= 8.8
duplicate strings are decided by script rule order. Example below:

.. code-block:: bash

  cat > 1.s << \!
  .section .rodata1.str1.1,"aMS",@progbits,1
  .string "abc"
  !

  cat > 2.s << \!
  .section .rodata1.str1.2,"aMS",@progbits,1
  .string "abc"
  !

  cat > script.t << \!
  SECTIONS {
    .rodata : {
        2.o(.rodata*)
        1.o(.rodata*)
     }
  }
  !

  # link order 1.o, 2.o
  # rule order 2.o, 1.o
  ld.eld 1.o 2.o

On 8.7, the string "abc" from 1.o will be included and the string from 2.o will
be merged with it (according to link order). On >=8.8, the string from 2.o will
be included and string from 1.o merged with it (according to rule order). This
difference is reflected in the map file.

Hexagon
=========

Convert binary file to Hexagon
-------------------------------

If you want to convert a binary file file1.bin to hexagon, you can use objcopy.

.. code-block:: bash

  hexagon-llvm-objcopy -I binary file1.bin -O elf32-hexagon test.o

Hexagon specific behavior
--------------------------

PHDRS
^^^^^^

Hexagon static link and dynamic linked executables dont rely on PHDR's to be
available for the dynamic loader.

If there is a need for you to load PHDR's, you need to look at
Gettingfileheaderandprogramheadertobeloaded

.. warning::

  **PHDRS not covered by load segment**

  Often times, if you are using readelf on a dynamic executable for hexagon, you may get an error

  readelf: Error: the PHDR segment is not covered by a LOAD segment

  This is because of the above assumption that Hexagon dynamic executables dont rely on PHDR's to be loaded and available to the dynamic loader.

  Use hexagon-llvm-readelf to overcome this error.

Why *COMMON* input section description does not affect all the common symbols?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For the hexagon target, by default, the linker maps small common symbols to
internal sections, .scommon.x, where x can be 1, 2, 4 and 8. The x represents
the size of the common symbol in bytes. For example, .scommon.4 will match common
symbols having size 4 bytes. Therefore, to write rules for small common symbols,
.scommon.x input section description should be used. This allows writing rules
affecting common symbols with greater precision

For Example, to match:

* all common symbols of size less than or equal to 2 bytes to the output section
  *.small_commons_2*
* all common symbols of size greater than 2 bytes but less than or equal to 8
  bytes to the output section *.small_commons_8.*

The following linker script SECTIONS command can be used:

.. code-block:: bash

  SECTIONS {
      .text : {*(.text*) }
      .small_commons_2: { *(.scommon.1 .scommon.2) }
      .small_commons_8: { *(.scommon.4 .scommon.8) }
      .bss: { *(COMMON) }
  }

\*(COMMON) input section description will match all common symbols of size greater than 8 bytes.

**To verify the linker script**

Test C code

.. code-block:: c

  char a;
  short b;
  int c;
  long long d;
  double e[100];

Compile and Link Step:

.. code-block:: bash

  clang -target hexagon -c test.c -o test.o
  hexagon-link test.o -o test.elf -T test.linker.script

Verify:

.. code-block:: bash

  hexagon-readelf -Ss common.elf

readelf output:

.. code-block:: bash

  There are 8 section headers, starting at offset 0x11c0:

  Section Headers:
    [Nr] Name              Type            Address  Off    Size   ES Flg Lk Inf Al
    [ 0]                   NULL            00000000 000000 000000 00      0   0  0
    [ 1] .small_commons_2  NOBITS          00000000 001000 000004 00  WA  0   0  2
    [ 2] .small_commons_8  NOBITS          00000008 001000 000010 00  WA  0   0  8
    [ 3] .bss              NOBITS          00000018 001000 000320 00  WA  0   0  8
    [ 4] .comment          PROGBITS        00000000 001000 000065 01  MS  0   0  1
    [ 5] .shstrtab         STRTAB          00000000 001065 00004b 00      0   0  1
    [ 6] .symtab           SYMTAB          00000000 0010b0 0000c0 10      7   6  4
    [ 7] .strtab           STRTAB          00000000 001170 00004a 00      0   0  1
  Key to Flags:
    W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
    L (link order), O (extra OS processing required), G (group), T (TLS),
    C (compressed), x (unknown), o (OS specific), E (exclude),
    R (retain), p (processor specific)

  Symbol table '.symtab' contains 12 entries:
     Num:    Value  Size Type    Bind   Vis       Ndx Name
       0: 00000000     0 NOTYPE  LOCAL  DEFAULT   UND
       1: 00000000     0 SECTION LOCAL  DEFAULT     1 .small_commons_2
       2: 00000000     0 SECTION LOCAL  DEFAULT     4 .comment
       3: 00000008     0 SECTION LOCAL  DEFAULT     2 .small_commons_8
       4: 00000018     0 SECTION LOCAL  DEFAULT     3 .bss
       5: 00000000     0 FILE    LOCAL  DEFAULT   ABS common.c
       6: 00000000     1 OBJECT  GLOBAL DEFAULT     1 a
       7: 00000002     2 OBJECT  GLOBAL DEFAULT     1 b
       8: 00000008     4 OBJECT  GLOBAL DEFAULT     2 c
       9: 00000010     8 OBJECT  GLOBAL DEFAULT     2 d
      10: 00000018   800 OBJECT  GLOBAL DEFAULT     3 e
      11: 00000339     0 NOTYPE  GLOBAL DEFAULT   ABS __end

readelf output shows what we expected:

* symbols a, and b are matched to the output section *.small_commons_2*
* symbols c, and d are matched to the output section *.small_commons_8*
* symbol e is matched to the output section *.bss.*

.. note::

  The linker option -G<size> can be used to specify the maximum size for the
  small common symbols. For example, -G4 option specifies the maximum size for the
  small common symbols to be 4 bytes. By default, the linker assumes the maximum
  size of the small common symbols to be 8 bytes.

How to disable small common symbol functionality?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Small common symbol functionality can effectively be disabled by using -G0 linker
command-line option. -G<size> option specifies the maximum size for the small
common symbols.

*-G0* will have the following effects:

* Small common symbols will map to COMMON input section description instead of
  .scommon.x input section descriptions in the linker script
* If not specified otherwise, small common symbols will be stored in .bss output
  section instead of .sdata output section.

RISC-V
=======

Show Linker relaxation output
------------------------------

The linker shows relaxation output by using the option --verbose. In future there
will be a better option to annotate where linker performed relaxation.

Disable Relaxation
--------------------

You can disable relaxation in the linker using the option, --no-relax. This option
disables all of linker relaxation except handling of alignment.

Disable compressed relaxation
------------------------------

You can disable linker relaxing to compressed instructions by using --no-c-relax flag.

Usage
======

**Response Files**

* Run ld.eld @<response-file> -o <elf>
  * Response-file contains the command line arguments. Any valid linker argument
    can be passed to the linker through the response-file and they are expanded
    and placed at "@<response-file>" position.
  * For e.g. ld.eld --help, the "--help" can go in a file x.cmd and passed to the linker using the '@' symbol.
    * **ld.eld @x.cmd -o <elf>**
  * @<response-file> removes the constraint of max command line length, so, a long
    list of arguments can be passed to the linker this way.
  * A "@response-file" can appear inside another response-file as well, allowing recursive processing of arguments.
  * Unrecognized arguments in the file will be ignored with a warning message, e.g. Warning: Unrecognized option '--bad'.
  * Invalid argument or missing response-file will cause linker to error out.
* Reference: https://llvm.org/docs/CommandLine.html#response-files

Linker Plugin Framework
=========================

What are the differences between SectionMatcherPlugin and SectionIteratorPlugin?
---------------------------------------------------------------------------------

* SectionMatcherPlugin is run before rule-matching and garbage-collection whereas
  SectionIteratorPlugin is run after rule-matching and garbage-collection.As a
  consequence of this, SectionIteratorPlugin callbacks have access to garbage-collected
  sections, discarded sections and rule matching information among other things.
* SectionIteratorPlugin does not call 'SectionIteratorPlugin::Process' callback
  hook for garbage-collected sections. It is though called for discarded sections.
  On the other hand, SectionMatcherPlugin call 'SectionMatcherPlugin::Process' callback
  hook for each input section.

Symbol Resolution
===================

Symbol wrapping
-----------------

What is symbol wrapping and how to use it?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symbol wrapping allows to use a wrapper symbol in-place of the original symbol,
without any modification to the source code. When symbol wrapping is used for
the symbol 'symbol', then all undefined references to 'symbol' is resolved
to '__wrap_symbol', and all undefined references to '__real_symbol' is resolved
to 'symbol'. To enable symbol wrapping, use '--wrap=symbol' option.

Where is symbol wrapping helpful?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symbol wrapping is typically used for wrapping system/standard function calls.
For example, a 'malloc' wrapper function can be used to keep track of bytes
allocated by 'malloc', and a 'printf' wrapper function can be used to add a
timestamp in the printf output.

Example
^^^^^^^^

The below program provides a wrapper function for 'my_malloc' function.
The wrapper function prints the number of bytes that is requested by user using
'my_malloc', and then calls 'my_malloc' to do the actual memory allocation.

.. code-block:: bash

  #!/usr/bin/env bash

  cat >1.c <<\EOF
  #include <stdio.h>

  void *my_malloc(size_t sz);

  int main() {
    int *p = my_malloc(sizeof(int)); // Resolves to __wrap_my_malloc
    *p = 11;
    printf("p: %d\n", *p);
    return *p;
  }
  EOF

  cat >2.c <<\EOF
  #include <stdlib.h>
  #include <stdio.h>

  void *my_malloc(size_t sz) { return malloc(sz); }

  void *__real_my_malloc(size_t sz);
  void *__wrap_my_malloc(size_t sz) {
    printf("'%u' bytes requested using my_malloc\n", sz);
    return __real_my_malloc(sz); // Resolves to 'my_malloc'
  }
  EOF

  clang -target hexagon -o 1.wrapped.elf 1.c 2.c 3.c -ffunction-sections -g -Wl,--wrap=my_malloc
  hexagon-sim 1.wrapped.elf
  # Output:
  # '4' bytes requested using my_malloc
  # p: 11

Build time issues
==================

Common to all targets
----------------------

LTO is showing undefined symbol when symbol is defined
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The questions to answer when there is an issue like this is to, see if the symbol
is in an archive library or an object file.

If its an archive library, make sure that the library is built using **llvm-ar**

The default archiver which handles ELF files does not handle **Bitcode** which is
the format that LLVM uses.

This is a must for LTO use.

If you have a reproducer using the reproduce option, you can use this snippet to
replace all your archive files using llvm-ar

.. code-block:: bash

  for file in `grep "\.lib" mapping.ini`; do file=`echo $file | sed 's/.*=//'`; echo $file; mkdir -p x; cd x; cp ../$file .; ar x $file; rm -f $file; llvm-ar cr $file *.o; cp $file ..; cd ..; done

LTO is showing could not set section name for the symbol
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This note is emitted while reading the bitcode files (only part of LTO flow, as
in non-LTO flow object files are read) for symbols defined using the '.set' directive.

.. note::

  extern void someSymbol_relocated (void) __attribute__((__visibility__("hidden")));

  __asm(".set someSymbol_relocated, 0x00005730");

This note means that no input section was found for this symbol in the input bitcode
file as these symbols are found to be undefined, due to assembler directive usage

The LTO flow, though, happens at link time but doesn't have visibility to mapping
symbol address set with .set directive to symbols.

Hexagon
========

Relocation overflows to function symbols
------------------------------------------

A relocation overflow can show up when the linker cannot insert a trampoline to
reach that symbol.

Most often the problems would be that the symbol may be *Undefined Weak.*

The programmer would have specified a *__attribute__((weak))* while declaring the function in the header file.

Also look at Weakreferencesanddefinitions why the linker was not seeing the definition.

.. note::

  A common work around for this sort of problem is to remove the __attribute__((weak)) from the prototype of the function.

Debugging Linker crash issues
-------------------------------

Step 1
^^^^^^^

Look for the error message produced by the linker, if it says Unexpected Linker
behavior continue to the next step.

If it shows the problem was in the user plugin, debug the plugin and figure out
what the issue is.

Step 2
^^^^^^^

Check to see if there any system environment variables set. Important variables to note are :-

* LD_LIBRARY_PATH
* PATH (Windows)

Remove the values set and see if the error disappears

Step 3
^^^^^^^

If the linker is still crashing, check to see if the cause is due to multithreading plugins.

You can then use --no-threads and see if the error disappears

Step 4
^^^^^^^

Sometimes the plugin may write some data into linker memory and corrupt some data structures.

Build the plugin with sanitizer enabled, and see if you can spot any sanitizer issues in the plugin.

Step 5
^^^^^^^

If it still is crashing then contact the support teams for help debugging the issue.

ARM
=====

SBREL relocations : Fixing relocation error when applying relocation 'R_ARM_SBREL32'
-------------------------------------------------------------------------------------

SBREL relocation stands for segment base relative relocations. There needs to be
only one segment defined that is segment base relative in the image.

All variables and access to the variables need to be placed in that segment.

Example and how to fix :-

.. code-block:: bash

  cat > 1.c << \!
  int foo;
  int bar = 20;
  int main() { return foo + bar; }
  !

  cat > script.t << \!
  PHDRS {
    TEXT PT_LOAD;
    SBREL_SEGMENT PT_LOAD;
    SOMETHING_ELSE PT_LOAD;
  }

  SECTIONS {
    .text : { *(.text) } :TEXT
    .ARM.exidx : { *(.ARM.exidx*) }
    .data : { *(.data.bar) } :SBREL_SEGMENT
    .bss : { *(COMMON) } :SOMETHING_ELSE
  }
  !

  clang -c -frwpi 1.c -target arm -fdata-sections -g3
  ld.eld -march arm 1.o -T script.t -Map x

Error: R_ARM_SBREL32 Relocation Mismatch for symbol bar defined in 1.o[.text] has a different load segment
Error: Relocation error when applying relocation `R_ARM_SBREL32' for symbol `bar' referred from 1.o[.text] symbol defined in 1.o[.data.bar]
Error: R_ARM_SBREL32 Relocation Mismatch for symbol bar defined in 1.o[.debug_info] has a different load segment
Error: Relocation error when applying relocation `R_ARM_SBREL32' for symbol `bar' referred from 1.o[.debug_info] symbol defined in 1.o[.data.bar]
Fatal: Linking had errors.

**Fixed linker script for this example**

.. code-block:: bash

  PHDRS {
    TEXT PT_LOAD;
    SBREL_SEGMENT PT_LOAD;
    SOMETHING_ELSE PT_LOAD;
  }

  SECTIONS {
    .text : { *(.text) } :TEXT
    .ARM.exidx : { *(.ARM.exidx*) }
    .data : { *(.data.bar) } :SBREL_SEGMENT
    .bss : { *(COMMON) } :SBREL_SEGMENT
  }

How to resolve the warning 'Compact Option needs physical address aligned with File offsets'
----------------------------------------------------------------------------------------------

Before we see how to resolve this warning, let's first discuss what is :option:`--compact` option and
the reason for this warning.

:option:`--compact` option, as the name suggests, allows to generate more compact (smaller) images.
However, this benefit comes with the added restriction that for each ``LOAD`` segment,
physical addresses must be aligned with the file offsets. This means the
the difference between physical address and file offset should should be the same
for each byte within a ``LOAD`` segment.

This implies that the physical addresses must also be aligned with
virtual memory addresses within a ``LOAD`` segment. This is because
the linker ensures that virtual memory addresses are always aligned with file offsets.

It is user's responsibility to ensure that this restriction is satisfied. The linker reports the warning
*Compact Option needs physical address aligned with File offsets* if the restriction is not satified.
This warning should not be ignored because images not satisfying this restriction may have runtime errors.

Let's analyze this issue with the help of an example:

.. code-block:: bash

	#!/usr/bin/env bash

	cat >1.c <<\EOF
	int foo = 1;

	__attribute__((aligned(8)))
	int bar = 3;

	int baz = 5;
	EOF

	cat >script.t <<\EOF
	PHDRS {
	  TEXT PT_LOAD;
	}

	SECTIONS {
	  FOO 0x1000 : AT(0x4) { *(*foo*) } :TEXT
	  BAR : { *(*bar*) } :TEXT
	  BAZ : { *(*baz*) } :TEXT
	}
	EOF

	clang -o 1.o 1.c -c -ffunction-sections -fdata-sections
	arm-link -o 1.out 1.o -T script.t -Map 1.map.txt --compact

We get the following warnings on running this example::

  Warning: Physical address and the offset of a segment must be congruent modulo the alignment of the segment. Mismatch found at segment FOO
  Warning: Compact Option needs physical address aligned with File offsets. Mismatch found at section BAR
  Warning: Compact Option needs physical address aligned with File offsets. Mismatch found at section BAZ

Why do we get this warning here and how do we fix it? Let's see.

For brevity, I will use VMA for virtual memory address and LMA (load memory address) for physical address.

``FOO`` size is 0x4, VMA is 0x1000 and LMA is 0x4. The difference between the
VMA and the LMA is 0xffc. We may expect ``BAR`` VMA to be 0x1004 (0x1000 + 0x4)
and the LMA to be 0x8 (0x4 + 0x4). However, the linker has to bump this VMA to 0x1008 to
satisfy ``BAR`` 8-byte alignment requirement. No bump is required for the LMA as it is
already 8-byte aligned. This bump to VMA causes misalignment between VMA and LMA, as
seen from VMA - LMA = 0x1000, whereas previously the difference was 0xffc. Hence we
also have misalignment between file offsets and LMA
(VMA and file offsets are *always* aligned) and the :option:`--compact` option
alignment restriction is violated!

Note that the linker cannot simply bump the LMA by 0x4 as well to align with the VMA bump because
then the LMA (0xc) will not be 8-byte aligned.

To fix this issue, we have to ensure that for each ``LOAD`` segment,
**the VMA is congruent to the LMA, modulo the maximum alignment of any section within the segment**.

For this particular case, this means that ``TEXT`` segment VMA and LMA must be congruent modulo 8.
8 is the alignment of ``BAR`` section and is the maximum alignment requirement of sections within ``TEXT``.
This congruency ensures that no bump for satisfying alignment will cause misalignment between VMA and LMA.
Hence, to fix this issue we can change VMA to 0x1004 (0x1004 and 0x4 are congruent to each other, modulo 8),
or change LMA to 0x8 (0x1000 and 0x8 are congruent to each other, modulo 8). Any other value of VMA and LMA
satifying this congruency is, of course, valid as well.

Runtime issues
===============

ARM
----

Crash with load / stores
--------------------------

Things to look at when there is a crash :-

* Look at the point of crash, and the memory from where the instruction is
  loading from or storing to
* If the load or store is happening with a variable stored in the stack
  * Bounce the problem to the compiler
* If not, these are the next steps
  * Look at variable, where its located from the Linker map file.
  * Check for alignment restrictions
  * Check for TLB permissions
  * Check Linker script, if linker script is overriding any default alignment for
    the section that the variable is housed in.
  * Check for page alignment
    * Sometimes the underlying operating system may require a higher page alignment
    * User can change the default page size using **-z max-page-size** option.

Improving your image/build
============================

The below section documents how users can improve the image / build by looking
into build time warning messages emitted by the linker.

Warnings
---------

Convert warning to error
^^^^^^^^^^^^^^^^^^^^^^^^^^

Users can add :code:`-Werror` to convert all warnings that the linker emits
into non-fatal errors.  A stronger form is :code:`--fatal-warnings`, which
promotes warnings to fatal errors.

This is synonymous to using :code:`-Wall -Werror` with the compiler

Non allocatable section assigned to an output section
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Users might want to avoid assigning a non allocatable section to an output section
which is allocatable.

Though this is not a serious issue to be looked into, some of the issues which
this will result in are :-

* It may be that tools may be not be able to decode or disassemble contents from
  the object files properly when there is a bug.
* The build also may become flaky
    * When section which converts the output section to be allocatable garbage
      collected, which will make the section that user added as a non allocatable
      section.
* Non allocatable sections are not loaded by the linker
* Relocations in non allocatable sections are not performed in the way allocatable
  sections are treated.

Linker detects that a non allocatable section is being assigned to an output
section which is allocatable.

.. code-block:: bash

  cat > code.s << \!
  .section .foo
  sym:
  nop
  .section .text
  call sym
  !

  cat > script.t << \!
  SECTIONS {
    .text : {
      *(.text)
      *(.foo)
    }
  }
  !

  $clang -c code.s
  $link code.o -T script.t -o code.out

Linker would issue the below warning in this case.

.. warning::

  Warning: Non-allocatable section '.foo' from input file 'code.o' is being merged
  into loadable output section '.text'

These warnings can be converted to errors with --fatal-warnings switch.

.. code-block:: bash

  $link code.o -T script.t --fatal-warnings

.. error::

  Fatal: Non-allocatable section '.foo' from input file 'code.o' is being merged
  into loadable output section '.text'

LinkerPlugin API
=================

Build Errors
-------------

Why am I receiving the error 'functions that differ only in their return type cannot be overloaded' error while building plugins?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Fix:** Build the plugin with '-std=c++14' (or greater C++ standard).

**But what is the cause of the error?**

The plugin framework overloads member functions based on the reference-qualifier
of 'this' argument to provide optimal performance. However, C++11 does not support
specifying reference-qualifier for the 'this' argument of member functions. This
means that when C++11 is used, C++ cannot distinguish between member function
prototypes that only differ due to the reference-qualifier of the 'this' argument.

Why am I getting an error about "Plugin Error referenced chunk <seen from last rule> deleted from Output section"
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This is a very common error, when a plugin takes chunks from linker script rules,
and sometimes the chunk are not put back by the linker plugins.

These chunks may be referred from other locations in the image, and is needed to
satisfy image layout requirements.

When such an error happens

* Please debug the plugin by inspecting calls to
    * addChunk
    * removeChunk
    * updateChunks

You may also rely on linker map file to see that chunks are added and removed
properly by inspecting plugin behavior.

Runtime errors
---------------

Why am I receiving the error 'Unable to load library ${libYourPlugin.so}: undefined symbol: ...'?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Fix:** Build the plugin with '-stdlib=libc++'.

But what is the cause of this runtime error?

Plugin framework is compiled with 'libc++' implementation of standard library.
Therefore, the mangled names of standard library components in the plugin framework
are as per 'libc++' implementation. By default, Clang uses 'libstdc++' implementation
of standard library, which has different mangled names of standard library components
as compared to 'libc++' implementation. This difference of C++ standard library
implementation causes the conflict in the symbol names which in turn causes
undefined symbol error.

Concepts
---------

What are the differences between SectionMatcherPlugin and SectionIteratorPlugin?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* *SectionMatcherPlugin* interface is run before garbage-collection whereas
  SectionIteratorPlugin interface is run after rule-matching and garbage-collection.
  As a consequence of this, SectionIteratorPlugin callbacks have access to the
  information which sections are garbage-collected and discarded.
* *SectionIteratorPlugin* interface does not call 'SectionIteratorPlugin::Process'
  callback hook for garbage-collected sections. It is though called for discarded
  sections. On the other hand, SectionMatcherPlugin call 'SectionMatcherPlugin::Process'
  callback hook for each input section.

What is the search order for plugin invocation configuration file?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Plugin invocation configuration file is the YAML file that is specified to the linker
using *--plugin-config <file>* option.

The linker searches this file as follows:

* If the *<file>* is an absolute path, then no search is performed and the path is
  taken as it is.
* If the *<file>* is a relative path, then the search is performed as follows:
    * In the current working directory. i.e. the directory from which linker is
      invoked.
    * In the search directories specified by the -L option, in the order they are
      listed in the link command.

What is the search order for LinkerWrapper::findConfigFile function?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

*LinkerWrapper::findConfigFile* searches the file as follows:

* If the *<file>* is an absolute path, then no search is performed and the path is taken as it is.
* If the *<file>* is a relative path, then the search is performed as follows:
    * In the current working directory. i.e. the directory from which linker is invoked
    * In the search directories specified by the -L option, in the order they are listed in the link command.
    * Plugin default configuration file directory that is shipped with the toolchain.
      This directory is different for each plugin that is shipped with the toolchain.
      This directory is searched so that it is convenient to use the default plugin
      configuration files.

Debugging
-----------

How to see which plugins are successfully loaded?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Plugin workflow can be traced using the option *'--trace=plugin'.*
This option tells the linker to print logs describing the plugin workflow, including
information about plugin loading.

Is it expected to see decrease in link-time performance when plugins are used?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Yes, user can is expected to see a decrease in link-time performance when

* Plugins are built with a wrong compiler optimization options.
    * Using -g in release builds
* Plugins are developed without using Multi-threading features available.

If you are developing plugins and plan to deploy it, use --print-timing-stats to
figure out which plugin / what part of the plugin is taking more time during the link step.
The plugin framework is feature rich and does not cause link time degradation in any way.

How to verify which plugin config file was selected by the linker?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Run the link command with --verbose option, and search the build logs for 'Trying to open.
*<plugin config file name>' and '<plugin config file name>.*: found'. These two
searches will tell you which directories were searched by the linker (and in which order),
and where the plugin configuration file was finally found.

Plugin compatibility
---------------------

Plugin API version
^^^^^^^^^^^^^^^^^^^^

Starting with version 19, linker verifies that the loaded plugin is build using
a compatible API version. Linker will refuse to load a plugin which API version
is incompatible or the plugin does not report its API version.

Plugin API version consists of major and minor version numbers. For example, the
current Plugin API version is 3.2, which can be inspected with the --version
linker option:

.. code-block:: bash

  $ ld.eld --version
  Supported Targets: hexagon
  Linker from QuIC LLVM Hexagon Clang version Version
  Linker based on LLVM version: 19.0
  Linker Plugin Support Enabled
  Linker Plugin Interface Version 3.2
  LTO Support Enabled

A plugin is compatible with given Plugin API version if their major version numbers
are equal and plugin's minor API version is the same or lower than the current
minor API version. This means that a plugin compiled for a certain linker API is
compatible with future linkers as long as the major version number stays the same.

Plugins must be recompiled to be used with linker with a different major API
version.

Reporting API version by plugins
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each plugin must report its API version by exporting the following function from
the plugin shared library:

.. code-block:: cpp

  extern "C" void getPluginAPIVersion(unsigned int *Major, unsigned int *Minor);

Plugin API library provides a helper implementation of this function, which as
defined in the PluginVersion.h  file. To report the plugin API, a plugin developer
can include this header file into one of the C++ source files:

.. code-block:: cpp

  #include <PluginVersion.h>

Linker Plugin Config Search Paths
----------------------------------

What are the search paths for linker plugin config files?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The *LinkerWrapper::findConfigFile()* API  searches for a config file in the
following directories in the following order:

* Try to find the file path directly
* Search directories passed to the linker via -L. Some directories are implicitly
  included, such as the current working directory, and /lib and /usr/lib on linux.
* Default path for config files:
  *${ORIGIN}/../etc/ELD/Plugins/<Plugin name>*

.. note::

  ${ORIGIN} is the directory in which the linker binary resides

How to verify that the plugin search config was found and debug related issues?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* The –verbose option will print extra logs to help determine if the plugin
  config file was found
* To debug issues related to search path for linker plugin file analyze the traces obtained via the below 2 steps:
    * To get the list of paths at which the plugin config file path was searched for grep for “Trying to open.*<plugin config file name>” on verbose logs.
    * To see if the linker plugin config file was found grep for “<plugin config file name>.*: found” on verbose logs.

Example:

An example of using the API and verbose option to debug search paths is shown below for linux and for windows:

.. code-block:: cpp

  eld::Expected<std::string> expConfigPath =
    getLinker()->findConfigFile("foo.ini");

Link command (linux):

.. code-block:: bash

  ld.eld -L/tmp/ ...

.. code-block:: bash

  // Search for the file directly
  Verbose: Trying to open `foo.ini' for plugin configuration INI file `foo.ini' (file path): not found
  // look in -L directories
  Verbose: Trying to open `/tmp/foo.ini' for plugin configuration INI file `foo.ini' (search path): not found
  // implicit directories (cwd, /lib, /usr/lib)
  Verbose: Trying to open `/usr2/aregmi/foo.ini' for plugin configuration INI file `foo.ini'
  Verbose: Trying to open `/lib/foo.ini' for plugin configuration INI file `foo.ini' (search path): not found
  Verbose: Trying to open `/usr/lib/foo.ini' for plugin configuration INI file `foo.ini' (search path): not found
  // try the default config directory
  Note: Using default config path for linker plugins
  Verbose: Trying to open `/local/mnt/workspace/aregmi/llvm-project/llvm/build/bin/../etc/ELD/Plugins/ConfigFile/foo.ini' for plugin configuration INI file `foo.ini' (search path): found
  Verbose: Found plugin config file /local/mnt/workspace/aregmi/llvm-project/llvm/build/etc/ELD/Plugins/ConfigFile/foo.ini

Link command (Windows):

.. code-block:: bash

  ld.eld -L/ C:\Users\ ...

Verbose logs:

.. code-block:: bash

  // Search for the file directly
  Verbose: Trying to open `foo.ini' for plugin configuration INI file `foo.ini' (file path): not found
  // look in -L directories
  Verbose: Trying to open `C:\Users\foo.ini' for plugin configuration INI file `foo.ini' (search path): not found
  // implicit directories (cwd)
  Verbose: Trying to open `C:\Users\aregmi\tmp\foo.ini' for plugin configuration INI file `foo.ini' (search path): not found
  Note: Using default config path for linker plugins
  // default config directory
  Verbose: Trying to open `C:\Users\aregmi\install\Tools\bin/../etc/ELD/Plugins/ConfigFile\foo.ini' for plugin configuration INI file
  `foo.ini' (search path): found
  Verbose: Found plugin config file C:\Users\aregmi\install\Tools\etc\ELD\Plugins\ConfigFile\config\foo.ini

Linker Script NOLOAD handling
-------------------------------

Description
^^^^^^^^^^^^

NOLOAD sections are used as a way to tell the loader to reserve one or more regions
in memory and not refresh its contents for repeated loads of the same image.

Assumptions of the regions are

* The region has a reserved virtual address space
* The region has a reserved physical address space
* The region gets translated to "BSS"

This means that the image when it runs, can write some content to the region and
re-read their contents.

Examples of use cases are:

* Store state of the application before the application crashes, resume from where
  it left off
* Store state of why the application crashed for inspection

References
^^^^^^^^^^^

https://mcuoneclipse.com/2014/04/19/gnu-linker-can-you-not-initialize-my-variable/

Usecases
^^^^^^^^^

NOLOAD region assigned to PT_NULL segment
""""""""""""""""""""""""""""""""""""""""""

Code:

.. code-block:: bash

  cat > fb.c << \!
  int foo() { return 0; }
  int bar() { return 0; }
  int baz() { return 0; }
  !

  cat > noload.lcs << \!
  PHDRS {
    A PT_LOAD;
    B PT_NULL;
  }

  SECTIONS {
    .foo : { *(.text.foo) } :A
    .bar (NOLOAD) : { *(.text.bar) } :B
    .baz (NOLOAD) : { *(.text.baz) } :B
  }
  !

The above test places bar and baz in NOLOAD regions and places them in a segment
that is non loadable (PT_NULL).

Readelf Output:

.. code-block:: bash

  $ llvm-readelf -l -W a.out

  Elf file type is EXEC (Executable file)
  Entry point 0x0
  There are 2 program headers, starting at offset 52

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x001000 0x00000000 0x00000000 0x0000c 0x0000c R E 0x1000
    NULL           0x000000 0x00000010 0x00000010 0x00000 0x00320 RW  0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .foo
     01     .bar .baz
     None   .comment .shstrtab .symtab .strtab

.. warning::

  PT_NULL segments in this case will have the virtual address set and the segment
  memory size appropriately set. If you do not want to see virtual addresses assigned
  or memory size set, remove the segment assignment(s).

Placing a loadable section to PT_NULL
""""""""""""""""""""""""""""""""""""""

Code:

.. code-block:: bash

  cat > fb.c << \!
  int foo() { return 0; }
  int bar[100] = { 0 };
  int baz[100] = { 0 };
  !

  cat > noload.lcs << \!
  PHDRS {
    A PT_LOAD;
    B PT_NULL;
  }

  SECTIONS {
    .foo : { *(.text.foo) } :A
    .bar (NOLOAD) : { *(.bss.bar) } :B
    .baz  : { *(.bss.baz) } :B
  }
  !

The test places baz which is loadable into a PT_NULL segment

Output:

.. error::

  Error: Loadable section .baz not in any load segment
  Fatal: Linking had errors.

Placing NOLOAD region to LOAD segment
""""""""""""""""""""""""""""""""""""""

Code:

.. code-block:: bash

  cat > fb.c << \!
  int foo() { return 0; }
  int bar[100] = { 0 };
  int baz[100] = { 0 };
  !

  cat > noload.lcs << \!
  PHDRS {
    A PT_LOAD;
    B PT_LOAD;
  }

  SECTIONS {
    .foo : { *(.text.foo) } :A
    .bar (NOLOAD) : { *(.bss.bar) } :B
    .baz (NOLOAD) : { *(.bss.baz) } :B
  }
  !

This tests produces valid results with a section that is set to NOLOAD and placed
in a PT_LOAD segment.

Output:

.. code-block:: bash

  $ llvm-readelf -l -W a.out

  Elf file type is EXEC (Executable file)
  Entry point 0x0
  There are 2 program headers, starting at offset 52

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x001000 0x00000000 0x00000000 0x0000c 0x0000c R E 0x1000
    LOAD           0x001010 0x00000010 0x00000010 0x00000 0x00320 RW  0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .foo
     01     .bar .baz

Mixing NOLOAD and LOAD regions
""""""""""""""""""""""""""""""""

Code:

.. code-block:: bash

  cat > fb.c << \!
  __attribute__((aligned(4))) int foo() { return 0; }
  __attribute__((aligned(4))) int bar() { return 0; }
  __attribute__((aligned(4))) int baz() { return 0; }
  __attribute__((aligned(4))) int bad() { return 0; }
  !

  cat > noload.lcs << \!
  PHDRS {
    A PT_LOAD;
    B PT_LOAD;
  }

  SECTIONS {
    .foo : { *(.text.foo) } :A
    .bar (NOLOAD) : { *(.text.bar) } :B
    .baz (NOLOAD) : { *(.text.baz) } :B
    .bad  : { *(.text.bad) } :B
  }
  !

This places text.bar and text.baz as NOLOAD and .text.bad as a loadable section
but all of them in a LOAD segment.

These are the things that happens

* The text section gets turned into a BSS section (without any warning)
* The text.bad section gets assigned a proper address

Output:

.. code-block:: bash

  $ llvm-readelf -l -W a.out

  Elf file type is EXEC (Executable file)
  Entry point 0x0
  There are 2 program headers, starting at offset 52

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x001000 0x00000000 0x00000000 0x0000c 0x0000c R E 0x1000
    LOAD           0x001010 0x00000010 0x00000010 0x0000c 0x0002c R E 0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .foo
     01     .bar .baz .bad

NOLOAD section in the middle of a PT_LOAD segment
""""""""""""""""""""""""""""""""""""""""""""""""""

.. code-block:: bash

  cat > 1.c << \!
  int foo() {
    return 0;
  }
  int mybss[100] = { 0 };

  int bar() {
    return 0;
  }
  !

  cat > script.t << \!
  PHDRS {
   A PT_LOAD;
  }
  SECTIONS {
   .foo : {
     *(.text.foo)
   } :A
   .bss (NOLOAD) : {
     *(.data*)
     *(.bss*)
   } :A
   .bar : {
    *(.text.bar)
   } :A
  }
  !

The example places foo and bar in .foo and .bar output sections respectively. The
linker script also reserves a .bss region of type NOLOAD, which is followed by a
loadable section .bar. All of the sections are located in a loadable segment by
name 'A'.

The layout is adjusted so that, the section .bar is placed after .bss in the file.
The offset for .bar is calculated as the offset of bss and the size of bss.

Output:

.. code-block:: bash

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x001000 0x00000000 0x00000000 0x001ac 0x001ac RWE 0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .f .bss .bar

.. warning::

  Without the NOLOAD directive, Linker would have produced this error message

  Mixing BSS and non-BSS sections in segment. non-BSS '.bar' is after BSS '.bss' in linker script

NOLOAD sections start of the segment and PT_NULL segment
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Code:

.. code-block:: cpp

  int foo() {
    return 0;
  }
  __attribute__((aligned(16384))) int bss1[100] = { 0 };
  __attribute__((aligned(16384))) int bss2[100] = { 0 };
  __attribute__((aligned(16384))) int bss3[100] = { 0 };
  __attribute__((aligned(16384))) int abss1[100] = { 0 };
  __attribute__((aligned(16384))) int abss2[100] = { 0 };
  __attribute__((aligned(16384))) int abss3[100] = { 0 };

  int bar() {
    return 0;
  }

LinkerScript:

.. code-block:: bash

  PHDRS {
   A PT_NULL;
  }
  SECTIONS {
   .abss1 (NOLOAD) : {
     *(.bss.abss1)
   } :A
   .abss2 (NOLOAD) : {
     *(.bss.abss2)
   } :A
   .abss3 (NOLOAD) : {
     *(.bss.abss3)
   } :A
   .f (NOLOAD) : {
     *(.text.foo)
   } :A
   .bss1 (NOLOAD) : {
     *(.bss.bss1)
   } :A
   .bss2 (NOLOAD) : {
     *(.bss.bss2)
   } :A
   .bss3 (NOLOAD) : {
     *(.bss.bss3)
   } :A
   .bar (NOLOAD) : {
    *(.text.bar)
   } :A
  }

Commands:

.. code-block:: bash

  $clang -c 1.c -ffunction-sections -fdata-sections -G0
  $ld.eld 1.o -T script.t

Output:

.. code-block:: bash

  $ readelf -l -W a.out

  Elf file type is EXEC (Executable file)
  Entry point 0x0
  There is 1 program header, starting at offset 52

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x000000 0x00000000 0x00000000 0x00000 0x1419c RWE 0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .abss1 .abss2 .abss3 .f .bss1 .bss2 .bss3 .bar

.. note::

  Note: the initial offset assigned to the section. This results in a bug with GNU linker.

NOLOAD sections start of the segment
""""""""""""""""""""""""""""""""""""""

Code:

.. code-block:: cpp

  int foo() {
    return 0;
  }
  __attribute__((aligned(16384))) int bss1[100] = { 0 };
  __attribute__((aligned(16384))) int bss2[100] = { 0 };
  __attribute__((aligned(16384))) int bss3[100] = { 0 };
  __attribute__((aligned(16384))) int abss1[100] = { 0 };
  __attribute__((aligned(16384))) int abss2[100] = { 0 };
  __attribute__((aligned(16384))) int abss3[100] = { 0 };

  int bar() {
    return 0;
  }

LinkerScript:

.. code-block:: bash

  PHDRS {
   A PT_LOAD;
  }
  SECTIONS {
   .abss1 (NOLOAD) : {
     *(.bss.abss1)
   } :A
   .abss2 (NOLOAD) : {
     *(.bss.abss2)
   } :A
   .abss3 (NOLOAD) : {
     *(.bss.abss3)
   } :A
   .f : {
     *(.text.foo)
   } :A
   .bss1 (NOLOAD) : {
     *(.bss.bss1)
   } :A
   .bss2 (NOLOAD) : {
     *(.bss.bss2)
   } :A
   .bss3 (NOLOAD) : {
     *(.bss.bss3)
   } :A
   .bar : {
    *(.text.bar)
   } :A
  }

Commands:

.. code-block:: bash

  $clang -c 1.c -ffunction-sections -fdata-sections -G0
  $ld.eld 1.o -T script.t

Output:

.. code-block:: bash

  $ readelf -l -W a.out

  Elf file type is EXEC (Executable file)
  Entry point 0x0
  There is 1 program header, starting at offset 52

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x004000 0x00000000 0x00000000 0x1419c 0x1419c RWE 0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .abss1 .abss2 .abss3 .f .bss1 .bss2 .bss3 .bar

How the offset of the section is calculated
''''''''''''''''''''''''''''''''''''''''''''''

* The offset of the first loadable section starts at an offset = previous offset +
  (current section address - first section in the segment)
* The offset of the first lodable section starts at an offset = previous offset +
  (current section address - previous section which occupies file space)

NOLOAD sections start of the segment with first load section starting at a virtual address
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Code:

.. code-block:: cpp

    int foo() {
    return 0;
  }
  __attribute__((aligned(16384))) int bss1[100] = { 0 };
  __attribute__((aligned(16384))) int bss2[100] = { 0 };
  __attribute__((aligned(16384))) int bss3[100] = { 0 };
  __attribute__((aligned(16384))) int abss1[100] = { 0 };
  __attribute__((aligned(16384))) int abss2[100] = { 0 };
  __attribute__((aligned(16384))) int abss3[100] = { 0 };

  int bar() {
    return 0;
  }

LinkerScript:

.. code-block:: bash

  PHDRS {
 A PT_LOAD;
  }
  SECTIONS {
   .abss1 (NOLOAD) : {
     *(.bss.abss1)
   } :A
   .abss2 (NOLOAD) : {
     *(.bss.abss2)
   } :A
   .abss3 (NOLOAD) : {
     *(.bss.abss3)
   } :A
   . = 0x80000000;
   .f : {
     *(.text.foo)
   } :A
   .bss1 (NOLOAD) : {
     *(.bss.bss1)
   } :A
   .bss2 (NOLOAD) : {
     *(.bss.bss2)
   } :A
   .bss3 (NOLOAD) : {
     *(.bss.bss3)
   } :A
   .bar : {
    *(.text.bar)
   } :A
  }

Commands:

.. code-block:: bash

  $clang -c 1.c -ffunction-sections -fdata-sections -G0
  $ld.eld 1.o -T script.t

Output:

.. code-block:: bash

  $ readelf -l -W a.out

  Elf file type is EXEC (Executable file)
  Entry point 0x0
  There is 1 program header, starting at offset 52

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x004000 0x00000000 0x00000000 0x8000c19c 0x8000c19c RWE 0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .abss1 .abss2 .abss3 .f .bss1 .bss2 .bss3 .ba

NOLOAD sections placed at beginning of segment
""""""""""""""""""""""""""""""""""""""""""""""""

When NOLOAD sections are placed at the beginning of the segment, they are not
assigned to any segment.

The sections are assigned NOBITS section property.

.. code-block:: bash

  cat > script.t << \!
  PHDRS {
    A PT_LOAD;
  }

  SECTIONS {
    .foo (NOLOAD) : {
              *(.text.foo)
              . = . + 0x4000;
           }
    .bar (NOLOAD) : { *(.text.bar) }
    .baz : { *(.text.baz) } :A
    /DISCARD/ : { *(.eh_frame) *(.ARM.exidx*) }
  }
  !

  cat > 1.c << \!
  int foo() { return 0; }
  int bar() { return 0; }
  int baz() { return 0; }
  !

.. code-block:: bash

  $clang -c 1.c -ffunction-sections -fdata-sections -G0
  $ld.eld 1.o -T script.t

.. code-block:: bash

  $ readelf -l -W a.out

  Elf file type is EXEC (Executable file)
  Entry point 0x0
  There is 1 program header, starting at offset 52

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x001020 0x00004020 0x00004020 0x0000c 0x0000c R E 0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .baz

LOAD sections following NOLOAD sections
""""""""""""""""""""""""""""""""""""""""

If there is a LOAD section and the the previous section was a NOLOAD section, the
load section is not assigned program headers automatically.

.. code-block:: bash

  cat > script.t << \!
  PHDRS {
    A PT_LOAD;
  }

  SECTIONS {
    .foo (NOLOAD) : {
              *(.text.foo)
              . = . + 0x4000;
           }
    .bar (NOLOAD) : { *(.text.bar) }
    .baz : { *(.text.baz) }
    /DISCARD/ : { *(.eh_frame) *(.ARM.exidx*) }
  }
  !

  cat > 1.c << \!
  int foo() { return 0; }
  int bar() { return 0; }
  int baz() { return
  !

.. code-block:: bash

  $clang -c 1.c -ffunction-sections
  $ld.eld 1.o -T script.t

.. warning ::

  Section .baz does not have segment assignment in linker script.

.. error ::

  Loadable section .baz not in any load segment

.. error ::

  Linking had errors.

NOLOAD sections at the beginning of the LOAD segment
""""""""""""""""""""""""""""""""""""""""""""""""""""""

NOLOAD sections when assigned to a LOAD segment gets loaded. The section is marked NOBITS.

.. code-block:: bash

  cat > script.t << \!
  PHDRS {
    A PT_LOAD;
  }

  SECTIONS {
    .foo (NOLOAD) : {
              *(.text.foo)
              . = . + 0x4000;
           } :A
    .bar (NOLOAD) : { *(.text.bar) } :A
    .baz : { *(.text.baz) } :A
    /DISCARD/ : { *(.eh_frame) *(.ARM.exidx*) }
  }
  !

  cat > 1.c << \!
  int foo() { return 0; }
  int bar() { return 0; }
  int baz() { return 0; }
  !

Commands:

.. code-block:: bash

  $clang -c 1.c -ffunction-sections
  $ld.eld 1.o -T script.t

Output:

.. code-block:: bash

  Section Headers:
    [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
    [ 0]                   NULL            00000000 000000 000000 00      0   0  0
    [ 1] .foo              NOBITS          00000000 001000 00400c 00  AX  0   0 16
    [ 2] .bar              NOBITS          00004010 001000 00000c 00  AX  0   0 16
    [ 3] .baz              PROGBITS        00004020 005020 00000c 00  AX  0   0 16
    [ 4] .comment          PROGBITS        00000000 00502c 000106 01  MS  0   0  1
    [ 5] .shstrtab         STRTAB          00000000 005132 000033 00      0   0  1
    [ 6] .symtab           SYMTAB          00000000 005168 0000a0 10      7   6  4
    [ 7] .strtab           STRTAB          00000000 005208 00002f 00      0   0  1
  Key to Flags:
    W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
    L (link order), O (extra OS processing required), G (group), T (TLS),
    C (compressed), x (unknown), o (OS specific), E (exclude),
    p (processor specific)

  Program Headers:
    Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
    LOAD           0x001000 0x00000000 0x00000000 0x0402c 0x0402c R E 0x1000

   Section to Segment mapping:
    Segment Sections...
     00     .foo .bar .baz
