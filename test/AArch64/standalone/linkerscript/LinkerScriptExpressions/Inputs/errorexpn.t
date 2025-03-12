SECTIONS {
  .my_start : { }
  . = ADDR(.my_start);
  _myvar2 = ABSOLUTE(.my_start);
}
