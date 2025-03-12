        .globl  main
        .type   main,@function
patch:
        call    xxx_f
        call    xxx_g
        call    xxx_h
        .size   patch, .-patch
