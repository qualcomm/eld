PHDRS
{
        sp_rom PT_LOAD;

}

SECTIONS
{
    /* ROM kernel code */

    .text_kernel 0x0018420 :
    {
        *(.text)
        *(.text.*)
    } :sp_rom

    /* ROM kernel read-only data */

    .rodata_kernel :
        ALIGN (16)
    {
        ro_start = .;
        *(.rodata.*)
        ro_end  = .;
    } :sp_rom

    /DISCARD/ :
    {
        *(.ARM.attributes)
        *(.ARM.exidx)
        *(.ARM.exidx.*)
        *(.comment)
        *(.interp)
        *(.remove_me)
    }

}
