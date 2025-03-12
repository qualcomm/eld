SECTIONS {
  .data : { *(.data) }
  .foo : { . = . + 100; }
}

ASSERT(. > 0, "The value of . is zero!");
ASSERT(SIZEOF(.foo) == 0, "Size of section .foo is not zero!");
