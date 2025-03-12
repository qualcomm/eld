.section .text.baz
.type baz, %function
.global baz
baz:
.section .text.bar
.type bar, %function
.global bar
bar:
nop
.section .foo,"a",%progbits
.local sym
sym:
.size sym, 40