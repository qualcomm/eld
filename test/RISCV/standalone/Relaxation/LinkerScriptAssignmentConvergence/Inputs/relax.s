.option relax

    .section .text.foo1,"ax",%progbits
    .global foo1
foo1:
    call bar1

    .section .text.foo2,"ax",%progbits
    .global foo2
foo2:
    call bar2

    .section .text.bar,"ax",%progbits
    .global bar1
bar1:
    .space 6
    ret

    .global bar2
bar2:
    ret
