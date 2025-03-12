SECTIONS
{
   .output(0x1500) : AT(0x10) {
    .= . + 50;
    *(.text.foo)}
}
