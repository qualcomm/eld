  .section ".text","ax",%progbits
  .global call_foo_from_outside
call_foo_from_outside:
  addi t0, x0, 0x1
  bne x0, t0, foo
  ret
