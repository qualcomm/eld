.. _command.rst:

.. Command

.. index:: command

*******
Command
*******

To start the linker from the command line:
::

	hexagon-link [option...] [input_fil...]

Where [*option...*] is one or more of the following:
::

    	@file
	--align-segments | --no-align-segments
	--allow-bss-conversion
	--allow-incompatible-section-mix
	--allow-multiple-definition
	--allow-shlib-undefined | --no-allow-shlib-undefined
	--as-needed | --no-as-needed
	-Bgroup
	-Bdynamic	|	-dy
	-Bshareable	|	-shared
	-Bstatic	|	-dn	|	-non_shared	|	-static
	-Bsymbolic
	-Bsymbolic-functions
	-build-id=style
	-color
	--color-map
	--copy-farcalls-from-file=value
	--copy-dt-needed-entries
	-cref
	-d
	-dc
	-default-script=file
	--defsym symbol=expression
	--demangle | --no-demangle
	--demangle-style=value
	--disable-guard-for-weak-undef
	--disable-new-dtags
	-discard-all
	-discard-locals
	-display-hotness
	-dp
	-dynamic
	--dynamic-linker=file
	--dynamic-list=file
	-eh-frame-hdr
	-EL
	--emit-relocs | --no-emit-relocs
	--emit-relocs-llvm
	--emit-timing-stats
	--enable-new-dtags
	--enable-threads=(all)
	--entry=entry	|	-e entry
	--error-style
	--exclude-libs=file
	--exclude-lto-filelist=list_of_files
	--export-dynamic	|	-E
	--export-dynamic-symbol=symbol_name
	-extern-list=file
	-fatal-warnings	|	--no-fatal-warnings
	-filetype=(obj|dso|exe)
	-fini=name
	-flto
	--flto-options=lto_options
	--force-dynamic
	-fPIC
	-g
	--gc-cref
	--gc-sections | --no-gc-sections
	-gpsize size	|	-G size
	-hash-size=size
	-hash-style=(sysv|gnu|both
	--help
	--help-hidden
	--ignore-unknown-opts
	--include-lto-filelist=list_of_files
	--image-base=image_base
	-init=name
	-larchive
	--library=namespec	|	-lnamespec
	--library-path=searchdir	|	-Lsearchdir
	-m=emulation
	-mabi=target_abi
	-M
	-Map=mapfile
	-MapDetail <value>
	-MapStyle <value>
	-march=version	|	-mcpu=version
	-merge-strings | --no-merge-strings
	-mllvm=options
	-mtriple=(hexagon-unknown-elf|hexagon-unknown-linux)
	-mv5	|	-mv55
	-mv60	|	-mv61	|	-mv62	|	-mv65	|	-mv66
	-mv67	|	-mv67t	|	-mv68
	--nmagic
	--no-reuse-trampolines-file=filename
	--no-trampolines
	--no-verify
	--no-warn-mismatch
	--no-warn-shared-textrel
	--noinhibit-exec
	-no-undefined
	-nostdlib
	--omagic | --no-omagic
	--opt-record-file
	--output=output	|	-o output
	-p
	-pie
	--plugin_opt
	--plugin
	--plugin-config=value
	--portable=symbol
	--print-gc-sections
	--print-map	|	-M
	--print-timing-stats
	--progress-bar
	-Qy
	--relocatable	|	-r
        -relocation-model=(default|static|pic|dynamic-no-pic)
	--repository-version
	--rosegment
	-rpath=pathname
	-rpath-link=pathname
	--save-temps
	--script=file	|	-T file
	--script-options=(match-gnu|match-llvm)
	--section-start section=org
	-soname=name	|	-h name
	--start-group=archive ... --end-group	|	-( archive ... -)
	--strip-all
	--strip-debug	|	-S
	-symdef <file>
	-sysroot=pathname
	-T=filename
	-Tbss=address
	-Tdata=address
	-threads | --no-threads
	--thread-count=no_of_threads
	-trace=symbol=<symbolname>
	--trace=type	|	-t=type
	-trampoline-map=value
	-Ttext=address
	-Ttext-segment=address
	--undefined=symbol	|	-u symbol
	--unresolved-symbols=method
	-use-memory
	-use-shlib-undefines
	--verbose | -verbose=type
	--verify-options=value
	-version
	--version-script=file
	--warn-common
	--warn-once
	--whole-archive	|	--no-whole-archive
	--wrap=symbol
	--just-symbols=file | -R file
	-Y=path
	-z keyword

Except for the command name, the linker is invoked like the GNU linker, and it supports the most commonly-used options in the GNU linker.
Arguments specified on the command line with no option switch are assumed to be object files. If the linker does not recognize these files as object files, it treats them as linker script files (Section 4).

.. note:: The @file option allows linker options and input files to be specified in a text file rather than directly on the command line.

