/*
This linker script distributes text and data of 5 files
(1.o, 2.o, 3.o, 4.o and 5.o) into separate sections using EXCLUDE_FILE
linker script directive. All the input section descriptions in the linker
script uses the '*' wildcard for matching files. The files are excluded
by using EXCLUDE_FILE.
*/
SECTIONS {
  ONE_TEXT : {
    EXCLUDE_FILE(*2.o *3.o) *(EXCLUDE_FILE(*3.o *4.o *5.o) *text*)
  }
  ONE_DATA : {
    EXCLUDE_FILE(*2.o *3.o) *(EXCLUDE_FILE(*3.o *4.o *5.o) *data*)
  }

  TWO_TEXT : {
    EXCLUDE_FILE(*3.o *4.o) *(EXCLUDE_FILE(*5.o) *text*)
  }
  TWO_DATA : {
    EXCLUDE_FILE(*3.o *4.o) *(EXCLUDE_FILE(*5.o) *data*)
  }

  THREE_TEXT : {
    EXCLUDE_FILE(*4.o) *(EXCLUDE_FILE(*5.o) *text*)
  }
  THREE_DATA : {
    EXCLUDE_FILE(*4.o) *(EXCLUDE_FILE(*5.o) *data*)
  }

  FOUR_TEXT : { EXCLUDE_FILE(*5.o) *(*text*) }
  FOUR_DATA : { EXCLUDE_FILE(*5.o) *(*data*) }

  OTHERS_TEXT : { *(*text*) }
  OTHERS_DATA : { *(*data*) }
}