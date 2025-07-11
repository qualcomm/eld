SECTIONS {
  INCLUDE i.t
  .RULE_MATCHING_BUG : {
    *(.text.foo)
  }
}