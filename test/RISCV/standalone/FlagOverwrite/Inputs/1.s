.section .text.foo
.globl foo
.type foo, @function
foo:
	lui	a5,%hi(bar)
	lw	a5,%lo(bar)(a5)
	beqz	a5,.L4
	call	baz
.L4:
	nop

.section .text.baz
.globl baz
.type baz, @function
baz:
  nop
.data
.globl bar
bar:
 .word 1
