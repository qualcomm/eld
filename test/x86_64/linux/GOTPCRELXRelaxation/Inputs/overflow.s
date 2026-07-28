        .globl sym
        .hidden sym
        .data
sym:    .long 42

        .text
        .globl h
        .type  h,@function
h:
        movl    sym@GOTPCREL(%rip), %eax
        ret
