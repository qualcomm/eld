/*=============================================================================

                                    qurt_default.lcs

GENERAL DESCRIPTION

EXTERNAL FUNCTIONS
        None.

INITIALIZATION AND SEQUENCING REQUIREMENTS
        None.

             Copyright (c) 2013  by Qualcomm Technologies, Inc.  All Rights Reserved.
=============================================================================*/
        SECTIONS {
             .qurtstart : {}
 . = ADDR(.qurtstart);

                
            
            /* START_SECTION */
            
             . = ALIGN(1M);
 .qstart : { KEEP(*crt0.o(.start)) }
 .qskshadow_main : { KEEP(*(.qskshadow_main)) }
 .qskshadow_ranges : { KEEP(*(.qskshadow_ranges)) }
 .qskshadow_island : { KEEP(*(.qskshadow_island)) }
 .qskshadow_tcm : { KEEP(*(.qskshadow_tcm)) }
 .qskshadow_coldboot : { KEEP(*(.qskshadow_coldboot)) }
 .qskshadow_zi_main : { KEEP(*(.qskshadow_zi_main)) }
 .qskshadow_zi_island : { KEEP(*(.qskshadow_zi_island)) }
 .qskshadow_zi_tcm : { KEEP(*(.qskshadow_zi_tcm)) }
 .qskshadow_zi_coldboot : { KEEP(*(.qskshadow_zi_coldboot)) }
 .qskshadow_zipages_main : { KEEP(*(.qskshadow_zipages_main)) }
 .qskshadow_zipages_island : { KEEP(*(.qskshadow_zipages_island)) }
 .qskshadow_zipages_tcm : { KEEP(*(.qskshadow_zipages_tcm)) }
 .qskshadow_zipages_coldboot : { KEEP(*(.qskshadow_zipages_coldboot)) }
 . = ALIGN(4K);
 .qskshadow_eip : { KEEP(*(.qskshadow_eip)) }
 .qskshadow_exports : { KEEP(*(.qskshadow_exports)) }
 . = ALIGN(4K);
 .qskstart : { KEEP(*(.start)) }
 . = ALIGN(4K);

            
            .init : {
                KEEP(*(.init))
            } =0x00C0007F
            .text : {
                *(.text *.text.*)
            }
            .fini : {
                KEEP(*(.fini))
            } =0x00C0007F
            .rodata : {
                *(.rodata .rodata.*)
            }
            .eh_frame : {
                KEEP(*(.eh_frame))
            }
            
            . = MAX(., (ADDR(.rodata)+0xFFF) & (-0x1000));
            
            
            .ctors : {
                KEEP(*(.ctors))
            }
            .dtors : {
                KEEP(*(.dtors))
            }
            .data : {
                *(.data .data.*)
            }
            .bss : {
                /* Non-secure ZI data area */
                __bss_start = .;
                *(.bss .bss.*)
                *(COMMON)
               _end = .;
            }
            . = ALIGN(128);
            _SDA_BASE_ = .;
            .sdata : {
                *(.sdata .sdata.*)
                *(.sbss.4 .sbss.4.*)
                *(.gnu.linkonce.l*)
            }
            __sbss_start = .;
            .sbss : {
                *(.sbss .sbss.*)
                *(.scommon .scommon.*)
            }
            __sbss_end = .;
                
                     .qsvspace : {
   . = ALIGN(8M);
   . = . + 0x800000;
 }
 .qskernel_main : { KEEP(*(.qskernel_main)) }
 .qskernel_ranges : { KEEP(*(.qskernel_ranges)) }
 .qskernel_island : { KEEP(*(.qskernel_island)) }
 .qskernel_tcm : { KEEP(*(.qskernel_tcm)) }
 .qskernel_coldboot : { KEEP(*(.qskernel_coldboot)) }
 __kernel_bss_start = .;
 .qskernel_zi_main : { KEEP(*(.qskernel_zi_main)) }
 .qskernel_zi_island : { KEEP(*(.qskernel_zi_island)) }
 .qskernel_zi_tcm : { KEEP(*(.qskernel_zi_tcm)) }
 .qskernel_zi_coldboot : { KEEP(*(.qskernel_zi_coldboot)) }
 .qskernel_zipages_main : { KEEP(*(.qskernel_zipages_main)) }
 .qskernel_zipages_island : { KEEP(*(.qskernel_zipages_island)) }
 .qskernel_zipages_tcm : { KEEP(*(.qskernel_zipages_tcm)) }
 .qskernel_zipages_coldboot : { KEEP(*(.qskernel_zipages_coldboot)) }
 __kernel_bss_end = .;
 . = ALIGN(4K);
 .qskernel_eip : { KEEP(*(.qskernel_eip)) }
 .qskernel_exports : { KEEP(*(.qskernel_exports)) }
 . = ALIGN(4K);
 .qskernel_eip_build : { KEEP(*(.qskernel_eip_build)) }
 .qskernel_vspace : {
   . = ADDR(.qskernel_main)+0x800000;
 }

                
            end = .;
        }
