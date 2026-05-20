  .section .text,"ax",%progbits
  .global foo_impl
  .type foo_impl, @function

foo_impl:
  addi a0, x0, 0x11
  ret

  .size foo_impl, . - foo_impl

  .global foo_resolver
  .type foo_resolver, @function

foo_resolver:
  lui a0, %hi(foo_impl)
  addi a0, a0, %lo(foo_impl)
  ret

  .size foo_resolver, . - foo_resolver

  .global foo
  .type foo, @gnu_indirect_function
  .set foo, foo_resolver

  .global main
  .type main, @function
main:
  lui a0, %hi(foo)
  addi a0, a0, %lo(foo)
  call foo
  ret

  .size main, . - main
