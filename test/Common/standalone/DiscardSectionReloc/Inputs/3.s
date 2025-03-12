.section .text.foo, "ae", %progbits
.global foo
foo:
.word bar
.section .text
.global bar
bar:
.word foo
.section .text.baz
.global baz
baz:
.word 100
.section .debug_line
.word foo