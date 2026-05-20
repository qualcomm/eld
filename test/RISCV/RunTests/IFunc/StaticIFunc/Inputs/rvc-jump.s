  .option rvc
  .section ".text","ax",%progbits
  .global call_foo_from_outside
call_foo_from_outside:
  c.j foo
  nop
