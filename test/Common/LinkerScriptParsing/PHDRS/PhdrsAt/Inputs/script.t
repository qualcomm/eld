PHDRS {
  A PT_NULL;
  B PT_LOAD AT (0x1000);
}

SECTIONS {
  a : { *(*a*) } :A
  b : { *(*b*) } :B
}