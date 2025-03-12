SECTIONS {
  text : {
    *(.foo)
    /* .bar should end up at 0x14. */
    . = ALIGN(20);
    *(.bar)
    . = ALIGN(0x100);
    /* Make sure we don't warn on ALIGN(0) and '.' is unchanged. */
    . = ALIGN(0);
    *(.baz)
    . = ALIGN(0x100);
    /* Check that our warnings can cope with non-outermost ALIGNs */
    . = ALIGN(ALIGN(30));
  }
}
