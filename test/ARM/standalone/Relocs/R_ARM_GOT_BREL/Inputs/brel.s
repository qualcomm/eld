 .syntax unified
 .text
 .global $d
$d:
 .word sym(GOT)

 .data
 .global sym
sym:
 .word 0


