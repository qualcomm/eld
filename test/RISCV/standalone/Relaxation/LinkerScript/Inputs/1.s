        .text
        .globl  foo
        .type   foo,@function
foo:
        nop
        ret

bar:
        nop
        tail foo
        tail foo
        tail foo
        tail foo
        ret
