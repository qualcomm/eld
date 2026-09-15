    .text
    .global foo
    .extern bar
    .extern x
foo:
    adrp    x0, :tlsdesc:x
    ldr     x1, [x0, :tlsdesc_lo12:x]
    add     x0, x0, :tlsdesc_lo12:x
    .tlsdesccall x
    blr     x1
    b bar

    .section .tbss,"awT",@nobits
    .global x
x:
    .zero 4
