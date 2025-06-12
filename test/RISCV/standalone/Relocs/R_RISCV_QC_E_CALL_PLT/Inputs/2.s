
  .section .text.test_vis_default, "ax", @progbits
  .global test_vis_default
  .type test_vis_default, STT_FUNC
test_vis_default:
  ret
  .size test_vis_default, .-test_vis_default

  .section .text.test_vis_hidden, "ax", @progbits
  .global test_vis_hidden
  .hidden test_vis_hidden
  .type test_vis_hidden, STT_FUNC
test_vis_hidden:
  ret
  .size test_vis_hidden, .-test_vis_hidden

  .section .text.test_vis_internal, "ax", @progbits
  .global test_vis_internal
  .internal test_vis_internal
  .type test_vis_internal, STT_FUNC
test_vis_internal:
  ret
  .size test_vis_internal, .-test_vis_internal

  .section .text.test_vis_protected, "ax", @progbits
  .global test_vis_protected
  .type test_vis_protected, STT_FUNC
test_vis_protected:
  ret
  .size test_vis_protected, .-test_vis_protected
