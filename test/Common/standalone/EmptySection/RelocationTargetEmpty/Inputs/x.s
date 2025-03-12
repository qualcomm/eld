.section .text.foo
.global foo
.set foo, bar
.section .text.bar
bar:
.word 100
.section .text.baz
.section .text.c1
.word .text.baz
.section .text.empty