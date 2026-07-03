        .globl callee
        .hidden callee
        .type  callee,@function
        .text
callee:
        ret

        .globl h
        .type  h,@function
h:
        call *callee@GOTPCREL(%rip)
        ret
