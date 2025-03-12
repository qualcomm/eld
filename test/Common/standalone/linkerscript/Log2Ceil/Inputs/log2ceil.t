SECTIONS {
  log2ceil0 = LOG2CEIL(0);
  log2ceil1 = LOG2CEIL(1);
  log2ceil2 = LOG2CEIL(2);
  log2ceil3 = LOG2CEIL(3);
  log2ceil4 = LOG2CEIL(4);
  log2ceil100000000 = LOG2CEIL(0x100000000);
  log2ceil100000001 = LOG2CEIL(0x100000001);
  log2ceilmax = LOG2CEIL(0xffffffffffffffff);
  .text : { *(.text*) }
}
