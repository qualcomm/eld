  .text
  .global ifunc_impl
  .type ifunc_impl, %function
ifunc_impl:
  .byte 0
  .size ifunc_impl, .-ifunc_impl

  .global ifunc
  .type ifunc, %gnu_indirect_function
  .set ifunc, ifunc_impl
