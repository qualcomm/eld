SECTIONS {
  . = 0;
  .text :  { *(.text*) }
  . = 0x1000;
  foo : { *(*foo*) }
  .=0x2000;
  bar : { *(*bar*)
  . =  ABSOLUTE(0x3000);}

}
