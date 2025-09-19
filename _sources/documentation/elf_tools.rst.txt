ELF TOOLS
==========================


This sections documents llvm ELF command-line tools along with detailed examples of their usage


**llvm-readelf** <options> <file>

  Inspect ELF files

  - **-h**

    Display ELF header information:

    .. ifconfig:: 'Hexagon' in targets

      .. code-block:: bash

        $ llvm-readelf -h a.out
        ELF Header:
          Magic:   7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
          Class:                             ELF32
          Data:                              2's complement, little endian
          Version:                           1 (current)
          OS/ABI:                            UNIX - System V
          ABI Version:                       0
          Type:                              EXEC (Executable file)
          Machine:                           QUALCOMM DSP6 Processor
          Version:                           0x1
          Entry point address:               0x0
          Start of program headers:          52 (bytes into file)
          Start of section headers:          228768 (bytes into file)
          Flags:                             0x68
          Size of this header:               52 (bytes)
          Size of program headers:           32 (bytes)
          Number of program headers:         4
          Size of section headers:           40 (bytes)
          Number of section headers:         17
          Section header string table index: 14

  - **-S**

    List ELF Sections

    .. code-block:: bash

      $ llvm-readelf -S a.out
      There are 17 section headers, starting at offset 0x37da0:

      Section Headers:
        [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
        [ 0]                   NULL            00000000 000000 000000 00      0   0  0
        [ 1] .start            PROGBITS        00000000 001000 0046c8 00 WAX  0   0 64
        [ 2] .init             PROGBITS        00005000 006000 000064 00  AX  0   0 32
        [ 3] .text             PROGBITS        00006000 007000 018bcc 00  AX  0   0 4096
        [ 4] .fini             PROGBITS        0001ebe0 01fbe0 000030 00  AX  0   0 32
        ...
        [11] .sdata            PROGBITS        00027000 028000 000170 00 WAp  0   0 4096
        [12] .bss              NOBITS          00027170 028170 0017d8 00  WA  0   0  8
        [13] .comment          PROGBITS        00000000 028170 000083 00  MS  0   0  1
        [14] .shstrtab         STRTAB          00000000 0281f3 000081 00      0   0  1
        [15] .symtab           SYMTAB          00000000 028274 006d50 10     16 530  4
        [16] .strtab           STRTAB          00000000 02efc4 008dbd 00      0   0  1

  - **-l**

    List ELF segments:

    .. code-block:: bash

      $ llvm-readelf -l a.out

      Elf file type is EXEC (Executable file)
      Entry point 0x0
      There are 4 program headers, starting at offset 52

      Program Headers:
        Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
        LOAD           0x001000 0x00000000 0x00000000 0x046c8 0x046c8 RWE 0x1000
        LOAD           0x006000 0x00005000 0x00005000 0x1f658 0x1f658 R E 0x1000
        LOAD           0x026000 0x00025000 0x00025000 0x02170 0x03948 RW  0x1000
        GNU_RELRO      0x026000 0x00025000 0x00025000 0x00044 0x00044 RW  0x4

      Section to Segment mapping:
        Segment Sections...
        00     .start
        01     .init .text .fini .rodata .eh_frame .gcc_except_table
        02     .ctors .dtors .data .sdata .bss
        03     .ctors .dtors

  - **-s**

    Display the symbol table

    .. code-block:: bash

      $ llvm-readelf -s hello.o

        Symbol table '.symtab' contains 324 entries:
          Num:    Value  Size Type    Bind   Vis      Ndx Name
            0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND
            1: 00000000     0 FILE    LOCAL  DEFAULT  ABS hello.cc
            2: 00000090     0 NOTYPE  LOCAL  DEFAULT    7 GCC_except_table10
            3: 00000230     0 NOTYPE  LOCAL  DEFAULT    7 GCC_except_table100
            4: 00000240     0 NOTYPE  LOCAL  DEFAULT    7 GCC_except_table121
            5: 00000268     0 NOTYPE  LOCAL  DEFAULT    7 GCC_except_table128
            ...

  - **-r** = **--relocations**

    Display relocation entries

    .. code-block:: bash

        $ cat 1.c
        int foo() {
          printf("hello world\n");
          return 0;
        }
        $ clang -c 1.c
        $ llvm-readelf -r 1.o
        Relocation section '.rela.text' at offset 0x110 contains 3 entries:
        Offset     Info    Type                Sym. Value  Symbol's Name + Addend
        00000004  00000211 R_HEX_32_6_X           00000000   .rodata.str1.1 + 0
        00000008  00000217 R_HEX_16_X             00000000   .rodata.str1.1 + 0
        0000000c  00000401 R_HEX_B22_PCREL        00000000   printf

  - **--string-dump** <section>

    Treat the contents of a section as a string and print it

    .. code-block:: bash

      $cat 1.c
      int foo() {
        printf("hello world\n");
        return 0;
      }
      $clang 1.c -c
      $llvm-readelf -S 1.o
      There are 9 section headers, starting at offset 0x194:

      Section Headers:
        [Nr] Name              Type            Address  Off    Size   ES Flg Lk Inf Al
        [ 0]                   NULL            00000000 000000 000000 00      0   0  0
        [ 1] .strtab           STRTAB          00000000 000131 000061 00      0   0  1
        [ 2] .text             PROGBITS        00000000 000040 000018 00  AX  0   0 16
        [ 3] .rela.text        RELA            00000000 00010c 000024 0c      8   2  4
        [ 4] .rodata.str1.1    PROGBITS        00000000 000058 00000d 01 AMS  0   0  1
      ...
      $llvm-readelf --string-dump .rodata.str1.1 1.o
      String dump of section '.rodata.str1.1':
      [     0] hello world.

  - **--hex-dump** <section>

    Print the contents of a section as hex

      .. code-block:: bash

        $cat 1.c
        void foo() {printf("hello world\n")}
        $clang 1.c -c
        $llvm-readelf -S 1.o
        There are 9 section headers, starting at offset 0x194:

        Section Headers:
          [Nr] Name              Type            Address  Off    Size   ES Flg Lk Inf Al
          [ 0]                   NULL            00000000 000000 000000 00      0   0  0
          [ 1] .strtab           STRTAB          00000000 000131 000061 00      0   0  1
          [ 2] .text             PROGBITS        00000000 000040 000018 00  AX  0   0 16
          [ 3] .rela.text        RELA            00000000 00010c 000024 0c      8   2  4
          [ 4] .rodata.str1.1    PROGBITS        00000000 000058 00000d 01 AMS  0   0  1
        ...
        $llvm-readelf --hex-dump .rodata.str1.1 1.o
        Hex dump of section '.rodata.str1.1':
        0x00000000 68656c6c 6f20776f 726c640a 00 hello world..


**llvm-objdump** <options> <file>

  Disassembler

  - **-d**

  Show assembler contents of executable sections:

  .. ifconfig:: 'Hexagon' in targets

    .. code-block:: bash

      $ llvm-objdump -d a.out
      a.out:  file format ELF32-hexagon
      Disassembly of section .start:
      0000000000000000 start:
      0:       4c c0 00 58 5800c04c {  jump 0x98 }
      4:       3e c0 00 58 5800c03e {  jump 0x80 }
      8:       42 c0 00 58 5800c042 {  jump 0x8c }
      ...


  - **-r**

  Display relocation entries involved during linking:

  .. ifconfig:: 'Hexagon' in targets

    .. code-block:: bash

      $ llvm-objdump -d -r hello.o
      hello.o:        file format ELF32-hexagon
      Disassembly of section .text:
      0000000000000000 __cxx_global_var_init:
            0:       01 c0 9d a0 a09dc001 {  allocframe(#8) }
            4:       00 40 00 00 00004000 {  immext(#0)
                              00000004:  R_HEX_32_6_X .bss
            8:       02 c0 00 78 7800c002    r2 = ##0 }
                              00000008:  R_HEX_16_X   .bss
            c:       00 c0 62 70 7062c000 {  r0 = r2 }
            10:       ff e2 9e a7 a79ee2ff {  memw(r30+#-4) = r2 }
            14:       00 c0 00 5a 5a00c000 {  call 0x14 }
                            00000014:  R_HEX_B22_PCREL      _ZNSt8ios_base4InitC1Ev
            ...


**llvm-nm** <options> <file>

  Lists symbols in a file

  - **no args**

    list regular symbols

    .. code-block:: bash

      $cat 1.c
      int bar() { return 10; }
      int foo() { return 1 + bar(); }
      $clang -c 1.c
      $llvm-nm 1.o
      00000000 T bar
      0000000c T foo

  - **-D** = **-dynamic**

    List dynamic symbols

    .. code-block:: bash

      $cat 1.c
      int bar() { return 10; }
      int foo() { return 1 + bar(); }
      $clang 1.c -o lib.so -fPIC -shared
      $llvm-nm -D lib.so
      000003a0 T _fini
      00000240 T _init
      00000370 T bar
      0000037c T foo

  - **-C**

    List symbols with demangling

    .. code-block
      $ llvm-nm -C helloworld.o
      00000000 T main
              U printf


**llvm-objcopy** <options> <input file> <output file>

  Copy and translate binary files



  - **no args** <input file> <output file>

    Create identical copy of one file to another, perform no translation

    if no output file is specified, input file will be modified in place

    .. code-block:: bash

      $llvm-objcopy 1.o 1.o.copy

  - **--input-target** <bfdname>, **--output-target** <bfdname>

    translate from input target to output target

    .. code-block:: bash

      $file 1.o
      1.o: ELF 32-bit LSB relocatable, Intel 80386, version 1 (SYSV), not stripped
      $llvm-objcopy --input-target elf32-i386 --output-target elf32-x86-64 1.o 1.o.copy
      $file 1.o.copy
      1.o.copy: ELF 32-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped

  - **--add-section** <section=file>

    insert a section into the output file with name *section* and contents of *file*
    for ELF files, inserted section will be of type *SHT_NOTE* if section name
    begins with ".note" otherwise will have type *SHT_PROGBITS*

    .. code-block:: bash

      $cat insert
      hello world
      $llvm-objcopy 1.o 1.o.copy --add-section newSection=insert
      $llvm-readelf --string-dump newSection
      String dump of section 'newSection':
      [     0] hello world


**llvm-ar** <options>

  Combine several input files into an archive library that can be linked with a program

  - **-cr** <output file> <input files>

    Create an archive file containing given input files

    .. code-block:: bash

      $ llvm-ar -cr lib.a 1.o 2.o 3.o
      $ file lib.a
      lib.a: current ar archive

  - **-t** <archive>

    list archive objects in an archive file

    .. code-block:: bash

      $ llvm-ar -cr lib.a 1.o 2.o 3.o
      $ llvm-ar -t lib.a
      1.o
      2.o
      3.o

  - **-x** <archive> <files>

    extract files from archive

    .. code-block:: bash

      $ ls
      lib.a
      $ llvm-ar -t lib.a
      1.o
      2.o
      3.o
      $ llvm-ar -x lib.a 1.o 2.o
      $ ls
      1.o 2.o lib.a

**llvm-strings** <options> <files>

  Dump printable strings from binary files to stdout

  **no-args** <files>

    Dump printable strings from input file(s) to stdout.
    If no file is specified, stdin is used

    .. code-block:: bash

      $ cat 1.c
      $ void foo() {printf("hello world\n");}
      $ clang 1.c -c
      hello world
      clang version 8.0.0 (tags/RELEASE_800/final)
      .rela.text
      .comment
      .note.GNU-stack
      .llvm_addrsig
      printf
      .rela.eh_frame
      .strtab
      .symtab
      .rodata.str1.1

  **--print-file-name** <files>

    Display the file containing the string before each string

    .. code-block:: bash

      $llvm-strings 1.o a.out
      1.o: hello world
      ...
      1.o: .strtab
      1.o: .symtab
      1.o: .rodata.str1.1
      a.out: /lib64/ld-linux-x86-64.so.2
      a.out: libc.so.6
      a.out: printf
      a.out: __libc_start_main
      ...
      a.out: __frame_dummy_init_array_entry




