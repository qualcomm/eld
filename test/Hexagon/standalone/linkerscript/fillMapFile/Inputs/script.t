SECTIONS{
.text : {
 *(.text.foo)
 . = . + 16;
 *(.text.car)
} = 0xc0037f

.data : {
  *(.data)
  . = . + 100;
  . = ALIGN(4K);
} = 0

. = ALIGN(256k);
.blah : {
 . = . + 100;
} = 0x1234

.rodata : {
  *(.rodata*)
}
}
