### If there are no rules, empty sections are skipped during merging, so their
### merged section may appear in the output in the order which is different
### from the order of the input files that contain this section.
### This test illustrates the failure condition, but it does not reproduce it,
### because the .got section below will contain one fragment of size 0 and
### hasSectionData() only looks at the number of fragments but not at their
### size. This test would not fail before this change.
.section .got, "aw"

.section .got.plt, "aw"
.word 0xaa
.word 0xbb
.word 0xcc
