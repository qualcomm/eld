.section .text.foo
.globl  foo
.type   foo,%function
foo:
nop

.section .text.__gxx_personality_v0
.globl  __gxx_personality_v0
.type   foo,%function
__gxx_personality_v0:
nop