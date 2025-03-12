SECTIONS {
 .stubs : { *(*.text.*tls*) }
 .text : { *(.text*) }
}
