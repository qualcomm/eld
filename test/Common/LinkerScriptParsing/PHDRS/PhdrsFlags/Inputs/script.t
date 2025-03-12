PHDRS {
  A PT_NULL;
  B PT_LOAD FLAGS (8);
}

SECTIONS {
  a : { *(*a*) } :A
  b : { *(*b*) } :B
}