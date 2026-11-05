INCLUDE script6bar.t
INCLUDE script6bar.t

SECTIONS {
  .text : { *(.text*) }
  .data : { *(.data*) }
}
