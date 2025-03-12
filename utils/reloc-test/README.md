Recommended usage:

    PATH=$TOOLS/bin:$PATH ./reloc-test.py --arch=<arch> --ld=<linker> --output-dir=<output-directory> >report.txt

where
* `<arch>` - architecture, aarch64 is currently supported.
* `<linker>` - test linker invocation path, may include command line options. Example: `aarch64-linux-gnu-ld`.
* `<output-directory>` - directory where work files are created. A large number of files will be created in this directory.

The default path must include some tools:
* `yaml2obj`
* `ld.lld`, used as a default linker

The tool writes a report to standard output, which describes the result of each run along with some information extracted from the output.
The report is intended to be diff'ed for different linkers.

Example of the report for one relocation:

	RUN dynamic.dyndat.abs64
	STATUS OK
	DYNREL RELA[0] offset 0x2000
	  .data @0x0
	.got [0] 0x838
	.got.plt [0] 0x0
	.got.plt [1] 0x0
	.got.plt [2] 0x0
	.data @0x0 0xffffffffffffffff
	   object ABS64 0

where
* `RUN dynamic.dyndat.abs64` is the name of the run, consisting of 1) type of the executable - dynamic executable, 2) type of symbol - dynamically imported data (variable), 3) relocation type - `R_AARCH64_ABS64`;
* `STATUS OK` - indicates that the link was successful;
* `DYNREL RELA[0] offset 0x2000` - dynamic relocation was created at addres `0x2000`, pointing to the `.data` section and offset `0x0`;
* `.got [0] 0x838` - GOT table with one slot, having the value of `0x838`;
* `.got.plt [0] 0x0 ...` - GOT PLT table with 3 slots, populated with indicated values;
* `.data @0x0 0xffffffffffffffff... object ABS64 0` - a text relocation placed a value of `-1` at offset `0x0` of section `.data`, with a dynamic relocation of type `R_AARCH64_ABS64` referncing symbol `object` and addend `0`.
