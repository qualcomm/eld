    .text
here:
    jump there

    .section .text.foo,"ax",@progbits
there:
    .section .text.bar,"ax",@progbits
    .global another
another:
