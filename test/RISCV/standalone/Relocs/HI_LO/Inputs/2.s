.data
bar:
        .word   1
        .text
        .align  1
.text
        .globl  foo
        .type   foo, @function
foo:
        lui     a5,%hi(bar)
        sw      zero,%lo(bar)(a5)
