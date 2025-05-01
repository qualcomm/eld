SECTIONS {
  foo :
  {
    . = ALIGN(4);
  }
   FOODATA_LMA_START = SIZEOF(foo);
   FOODATA_LMA_END    = FOODATA_LMA_START + 10;
}
