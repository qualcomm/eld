.data
.global foo
.type foo, %object
.size foo, 8
foo:
  .xword 1

.text
.global main
.type main, %function
main:
  adrp x0, :got:foo
  ldr  x0, [x0, :got_lo12:foo]
  ret
