SECTIONS {
.text : { *(.text*) }
.script_data : { *(COMMON) }
.everything_else : {
*(*) }
}
