        .globl sym_hidden
        .hidden sym_hidden
        .data
sym_hidden: .long 42

        .globl sym_default
        .data
sym_default: .long 99

        .text
        .globl h_hidden
        .type  h_hidden,@function
h_hidden:
        movl    sym_hidden@GOTPCREL(%rip), %eax
        ret

        .globl h_default
        .type  h_default,@function
h_default:
        movl    sym_default@GOTPCREL(%rip), %eax
        ret
