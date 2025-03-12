SECTIONS {
  .text : { *(.text*) }
  .five : {
    BYTE(1);
    SHORT(4);
    BYTE(0);
   }
  .some_value : {
    LONG(15);
    LONG(11);
    LONG(5);
    BYTE(5);
    SHORT(30);
    SHORT(100);
  }
  .empty : {
    SHORT(0);
    LONG(0);
    LONG(0);
    SHORT(0);
    BYTE(0);
    BYTE(0);
  }
}