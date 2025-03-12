.section .text,"ax",@progbits
.p2align 2
.global main
main:
{
  r0 = #10
  r1 = #11
  p0 = cmp.eq(r0,#0)
  if (p0) jump #foo
}
{
  r0 = #10
  r1 = add(r0, r1)
  p0 = cmp.eq(r0,#21)
  if (p0) jump #bar
}
.section .text.foobar,"ax",@progbits
.p2align 2
foo:
jumpr r31
bar:
jumpr r31