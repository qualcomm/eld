                .text
                .file        "thread_local.cpp"
                .globl     main
                .falign
                .type     main,@function

                                r2 = memw(r3 + ##_ZZ21set_ctor_thread_localiiE5value@TPREL)
                                r2 = memw(r3 + ##_ZZ21set_ctor_thread_localiiE5value)
                                jumpr r31

                .type     _ZZ21set_ctor_thread_localiiE5value,@object // @_ZZ21set_ctor_thread_localiiE5value
                .section                .tbss,"awT",@nobits
                .p2align 2
_ZZ21set_ctor_thread_localiiE5value:
                .space   4
                .size       _ZZ21set_ctor_thread_localiiE5value, 4
