.section .text.foo
{ call bar }
{ call baz }
{ jump .text.boo }
.section .text.boo
boo:
jumpr r31
.section .text.bar
bar:
nop
.section .text.baz
baz:
nop
