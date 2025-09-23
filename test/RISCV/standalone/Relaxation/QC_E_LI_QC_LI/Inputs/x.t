
/* Make GP somewhere unreachable by QC.LI or C.LUI */
__global_pointer$ = 0x01000000;

can_c_lui    = 0x0001f000;
cannot_c_lui = 0x00000801;

can_qc_li    = 0x00040000;
cannot_qc_li = 0x00080001;

can_addi_gprel    = 0x01000020;
cannot_addi_gprel = 0x01002001;

can_lui    = 0x1100f000;
cannot_lui = 0x1100f001;

SECTIONS {
   .text : {
    *(.text)
   }
}
