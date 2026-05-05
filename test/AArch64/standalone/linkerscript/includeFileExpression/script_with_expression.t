PHDRS {
    A PT_LOAD;
}

SECTIONS
{
  .data_l1wb_l2uc :
  {
  } :A=0
  INCLUDE include.lcs
}

