.section .text.foo_group,"axG",%progbits,foo_group,comdat,unique,1
.weak f1
f1:
.word 0
.weak foo_group
foo_group:
.word 0
.section .text.foo_group,"axG",%progbits,bar_group,comdat,unique,2
.weak f2
f2:
.word 0
.weak bar_group
bar_group:
.word 0
.section .text.foo,"ax",@progbits,unique,3
.global foo
foo:
nop
.section .text.foo,"ax",@progbits,unique,4
.global bar
bar:
nop
