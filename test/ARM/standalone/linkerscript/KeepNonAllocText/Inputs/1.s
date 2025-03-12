        .text
        .file   "1.c"
        .section        .text.foo
        .globl  foo
        .type   foo,%function
foo:                                    // %foo
                b bar
.Lfunc_end0:
        .size   foo, .Lfunc_end0-foo

        .section        .text.bar
        .globl  bar
        .type   bar,%function
bar:                                    // %bar
                b foo
.Lfunc_end1:
        .size   bar, .Lfunc_end1-bar

        .section        .text.main,"ax",%progbits
        .globl  main
        .type   main,%function
main:                                   // %main
          b foo
.Lfunc_end2:
        .size   main, .Lfunc_end2-main
        .section        ".note.GNU-stack","",%progbits
