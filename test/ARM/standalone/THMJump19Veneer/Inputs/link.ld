MEMORY {
    FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 2M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

SECTIONS {
    .text 0x10000000 : {
        *(.text*)
    } > FLASH

    .data : { *(.data*) } > RAM AT > FLASH
    .bss  : { *(.bss*) *(COMMON) } > RAM
}
