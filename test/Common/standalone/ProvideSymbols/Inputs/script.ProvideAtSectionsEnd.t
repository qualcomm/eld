SECTIONS {
  PROVIDE(f4 = 0x1000);
  PROVIDE(f3 = f4);
  PROVIDE(f2 = f3);
  PROVIDE(f2 = f3);
  PROVIDE(f1 = f2);
  PROVIDE(PROVIDED_SYM = f1);
  .foo : { *(.text*) }
  tdata : { . = . + 100; }
  tbss  : { . = . + 100; }
  PROVIDE(__tdata_start = ADDR(tdata));
  PROVIDE(__tdata_size = SIZEOF(tdata));
  PROVIDE(__tdata_end = __tdata_start + __tdata_size);
}