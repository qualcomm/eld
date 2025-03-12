
PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
  .blah : {
    . = . + 158;
  } :A

  .mytext : AT(160) {
  } :B

  address_of_mytext = .;

  .data : {
    *(.data)
  } :B
}
