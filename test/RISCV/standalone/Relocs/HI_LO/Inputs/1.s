.section .text
.globl foo
.type foo, @function
foo:
    lui a1, %hi(str)
    addi a1, a1, %lo(str)

.section .rodata
str:
    .string "Test string\n"
