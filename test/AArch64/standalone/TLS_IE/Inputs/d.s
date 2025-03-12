        .global var
        .section        .tdata
var:
        .byte   8
        .global var2
        .section        .tdata
var2:
        .word   16

        .global tlsievar
        .section        .tbss,"awT",%nobits
        .align  2
        .type   tlsievar, %object
        .size   tlsievar, 4
tlsievar:
        .zero   4

        .global l_tlsievar
       .align  2
        .type   l_tlsievar, %object
        .size   l_tlsievar, 4
l_tlsievar:
        .zero   4

.text
_test_tls_IE:

        adrp x0, :gottprel:tlsievar
        ldr  x0, [x0, :gottprel_lo12:tlsievar]

        adrp x0, :gottprel:l_tlsievar
        ldr  x0, [x0, :gottprel_lo12:l_tlsievar]

_test_tls_IE_local:

        adrp x0, :gottprel:var
        ldr  x0, [x0, :gottprel_lo12:var]

        adrp x0, :gottprel:var2
        ldr  x0, [x0, :gottprel_lo12:var2]
