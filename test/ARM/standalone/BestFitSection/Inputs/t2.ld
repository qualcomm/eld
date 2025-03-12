SECTIONS
{
    /* ROM kernel code */

    .text_kernel 0x0018420 :
    {
        *(.text)
        *(.text.*)
    }

    /* ROM kernel read-only data */

    .rodata_kernel :
        ALIGN (16)
    {
        ro_start = .;
        *(.rodata.*)
        ro_end  = .;
    }

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
