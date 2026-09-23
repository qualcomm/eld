        .globl sym
        .hidden sym
        .data
sym:    .long 42

        .text
        .globl h
        .type  h,@function
h:
        movl 0(%rip), %eax
        .reloc .-4, R_X86_64_GOTPCREL, sym-4
        ret
