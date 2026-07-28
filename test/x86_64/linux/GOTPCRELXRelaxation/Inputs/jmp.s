        .globl target
        .hidden target
        .type  target,@function
        .text
target:
        ret

        .globl h
        .type  h,@function
h:
        jmp *target@GOTPCREL(%rip)
