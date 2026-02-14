.section .baz,"ax",@progbits
.global baz1
baz1:
call foo
.global baz2
baz2:
.align 16
call foo
.global baz3
baz3:
call foo
