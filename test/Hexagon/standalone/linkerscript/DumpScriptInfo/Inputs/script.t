PHDRS {
  ABCD  PT_LOAD;
  AB    PT_LOAD;
}

OUTPUT (abcd.out)
SEARCH_DIR(.)
OUTPUT_ARCH(hexagon)
OUTPUT_FORMAT(binary)
plusval = 0x100;
SECTIONS {
 .A : AT(0X250) {
*(.text.bar)
} :ABCD

.B(0x55) :
{
     data = LOADADDR(.A);
    *(.text.f2)
   _edata = ABSOLUTE(.) ;
 } :AB
}
NOCROSSREFS (.A .B)
