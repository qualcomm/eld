SECTIONS {
.mytext : { *(.text.*) }
.mysmallcommons1 : { *1.o(.scommon*) *1.o(COMMON) }
.mysmallcommons2 : { *2.o(.scommon*) }
.mybigcommons : { *2.o(COMMON) }
.allcommons : { *(COMMON) }
.allnote : { *(.note*) }
.comment : { *(.comment) }
/DISCARD/ : { *(.ARM*) }
/DISCARD/ : { *(.hexagon*) }
.unrecognized : { *(*) }
}
