SECTIONS {
  .my_section1 0x1000 : { }
  _my_section1_vma = ADDR(.my_section1);
  _my_section1_lma = LOADADDR(.my_section1);

  .my_section2 0x3000 : AT(0x4000) { }
  _my_section2_vma = ADDR(.my_section2);
  _my_section2_lma = LOADADDR(.my_section2);

  .my_section3 (ADDR(.my_section1) + 0x4000) : AT (LOADADDR(.my_section2) + 0x4000) {
    _my_section3_vma = .;
  }
  _my_section3_lma = LOADADDR(.my_section3);
}
