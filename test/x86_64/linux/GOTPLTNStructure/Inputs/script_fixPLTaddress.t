#Need to fix the address of the .plt section as we are using absolute value checks to verify the correctness of GOTPLTN entries.
#These absolute values are otherwise sensitive to get changed with updates made in future PRs
SECTIONS {
  .plt 0x400280 : {
    *(.plt)
  }
}