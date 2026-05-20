  .section ".text","ax",%progbits
  .global call_foo_from_outside
call_foo_from_outside:
  jal x0, foo
  ret
