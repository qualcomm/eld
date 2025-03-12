SECTIONS {
   .text (0x1233000) : {
    *(.text*)
   }
   .sdata (0x1234560) : {
     __global_pointer$ = . + 0x800;
     *(.rodata.*)
   }
}
