SECTIONS {
TEXT :

{ *(*text*) }
. = ALIGN(ALIGNOF(NON_EXISTENT_SECTION));
DATA :

{ *(*data*) }
}
