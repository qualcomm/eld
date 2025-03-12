PHDRS{
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
    seca 0x20000 : {
        KEEP(*.o (.text))
    } : A

    secb ALIGN(128): {
        * (.bss .common COMMON)
    } : B

    secc : {
      . = . + 0x20;
    }

    secd ALIGN(4096): {
      . = . + 0x10000;
    }

    sece  : {
      . = . + 0x100000;
    }
}