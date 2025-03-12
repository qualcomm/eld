SECTIONS
{
  .text.hot : {
     1*.o (.text.eldfn*)
  }

  .text.cold : {
     lib11.a:11.o (.text.*main)
  }

  .text : { *(.text*) }

  .data : { *(.data.*) }
}
