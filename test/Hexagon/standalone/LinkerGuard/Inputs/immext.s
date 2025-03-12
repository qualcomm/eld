.text
{
  r0 = add(pc, ##foo@PCREL)
  if (cmp.eq(r0.new,#0)) jump:t 0
}
.data
.weak foo
