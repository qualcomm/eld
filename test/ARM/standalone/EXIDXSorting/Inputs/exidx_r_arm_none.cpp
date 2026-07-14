// f2: no exceptions — compiler emits only R_ARM_PREL31 at EXIDX offset 0.
// sortEXIDX() derives the sort key directly from that relocation.
void f2() {}

// f1: throws — compiler emits R_ARM_NONE (personality hint) followed by
// R_ARM_PREL31 at EXIDX offset 0.  sortEXIDX() must skip R_ARM_NONE and
// use R_ARM_PREL31 as the sort key; if it mistakenly used R_ARM_NONE's
// symValue (__aeabi_unwind_cpp_pr0 address) the entry would sort before f2
// even though f1 has a higher function address.
void f1() { throw 1; }
