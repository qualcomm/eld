        .global tlsievar
        .section        .tbss,"awT",%nobits
        .align  2
        .type   tlsievar, %object
        .size   tlsievar, 4
tlsievar:
        .zero   4
.text
_test_tls_ie_global:
        adrp x0, :gottprel:tlsievar
        ldr  x0, [x0, :gottprel_lo12:tlsievar]
