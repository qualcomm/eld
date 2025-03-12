.section .text.foo
.globl foo
.type foo, @function
foo:
  call bar

.section .text.bar
.globl bar
.type bar, @function
bar:
  nop
