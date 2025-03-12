SECTIONS {
  .blah : {
    . = . + 158;
  }

  .mytext : AT(160) {
  }

  address_of_mytext = .;

  .data : {
    *(.data)
  }
}
