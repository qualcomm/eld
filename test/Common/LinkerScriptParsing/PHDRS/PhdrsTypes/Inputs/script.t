PHDRS {
  A PT_NULL;
  B PT_LOAD FILEHDR PHDRS;
  C PT_DYNAMIC;
  D PT_INTERP;
  E PT_NOTE;
  F PT_SHLIB;
  G PT_PHDR;
  H PT_TLS;
}

SECTIONS {
  a : { *(*a*) } :A
  b : { *(*b*) } :B
  c : { *(*c*) } :C
  d : { *(*d*) } :D
  e : { *(*e*) } :E
  f : { *(*f*) } :F
  g : { *(*g*) } :G
  h : { *(*h*) } :H
}