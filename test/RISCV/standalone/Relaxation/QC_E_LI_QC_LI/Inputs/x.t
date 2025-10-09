
/* Make GP somewhere unreachable by QC.LI */
__global_pointer$ = 0x01000000;

can_qc_li    = 0x00040000;
cannot_qc_li = 0x00080000;

can_addi_gprel    = 0x01000020;
cannot_addi_gprel = 0x01002000;

SECTIONS {
   .text : {
    *(.text)
   }
}
