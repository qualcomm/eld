/* BEFORE_SECTIONS assignment */
ABS1 = 0x1234;

SECTIONS
{
  /* Place .text and emit assignments inside the output section */
  .text :
  {
    *(.text*)
    ABS2 = .;
    ABS3 = .;
  }

  /* OUTPUT_SECTION(EPILOGUE) assignment */
  .text : { }
  ABS4 = .;
}

/* AFTER_SECTIONS assignment */
ABS5 = 0x5678;

