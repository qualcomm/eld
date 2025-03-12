.section .text.foo
.global foo
foo:
.word local
local:
.word 100

.section .text.far
.global far
far:
jumpr r31

.section .text.bar
.global bar
bar:
{call far}