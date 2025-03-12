foo_text_reserved = 200;
SECTIONS {
  foo_text_start = .;
  .text.foo : {
    foo_text_size = foo_text_end - foo_text_start;
    foo_text_diff = foo_text_reserved - foo_text_size;
    . += (foo_text_size >= foo_text_reserved ? 0x0 : foo_text_diff);
  }
  foo_text_end = .;
  /DISCARD/ : { *(.ARM.exidx*) }
}

