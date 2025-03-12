        .text
        .type   foo,%object
        .section        .rodata.foo,"aMS",%progbits,1
        .weak   foo
        .align  4
foo:
        .word   10
        .size   foo, 4
        .section        .rodata.bar,"aw",%progbits
        .weak   bar
        .align  4
bar:
        .word   10
        .size   bar, 4

