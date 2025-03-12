SECTIONS {
.text : { *(.text*) }
.data : { *(COMMON) }
.sdata : { *(.scommon*) }
}
