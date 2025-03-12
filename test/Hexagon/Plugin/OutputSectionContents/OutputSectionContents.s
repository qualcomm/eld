#---OutputSectionContents.s--------------------- Executable---------------------#

## BEGIN_COMMENT
# Uses a plugin to retrieve the contents of an output section
## END_COMMENT

.section .nobits, "wa", @nobits

.section .rodata.str1.1,"aMS",%progbits,1
.string "hello\n"
.string "world\n"

## START_TEST
# RUN: %clang %clangopts -c %s -o %t.o
# RUN: %not %link %linkopts -T %p/Inputs/script.t %t.o -o %t.out 2>&1 | %filecheck %s
## END_TEST

# CHECK: hello
# CHECK-NEXT: world
# CHECK-NEXT: aaaabbbb
# CHECK-NEXT: Cannot retrive content of NOBITS section .buffer