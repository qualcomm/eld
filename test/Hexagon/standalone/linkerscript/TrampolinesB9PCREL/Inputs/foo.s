.section .text.foo
.global foo
foo:
{
p0 = cmp.eq(r0,#0); if (p0.new) jump:nt #far
}
