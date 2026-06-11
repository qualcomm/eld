SECTIONS
{
  .text.hot : {
     1*.o (.text.eldfn*)
  }

  .text.cold : {
    INCLUDE include.t
  }

  .text :: { *(.text*) }

  .data : { *(.data.*) }
}
