SECTIONS {
  .my_start : { }
  . = ADDR(.my_start);
  _myvar2 = ALIGNOF(.my_start);
  _myvar3 = ABSOLUTE(_myvar2);
}
