        .section .text,"axG",%progbits,foo_group,comdat
        .weak foo
foo:
        .word 0

.macro gen_sections4 x
        .section .text.a\x, "ax", %progbits
        .section .text.b\x, "ax", %progbits
        .section .text.c\x, "ax", %progbits
        .section .text.d\x, "ax", %progbits
.endm

.macro gen_sections8 x
        gen_sections4 a\x
        gen_sections4 b\x
.endm

.macro gen_sections16 x
        gen_sections8 a\x
        gen_sections8 b\x
.endm

.macro gen_sections32 x
        gen_sections16 a\x
        gen_sections16 b\x
.endm

.macro gen_sections64 x
        gen_sections32 a\x
        gen_sections32 b\x
.endm

.macro gen_sections128 x
        gen_sections64 a\x
        gen_sections64 b\x
.endm

.macro gen_sections256 x
        gen_sections128 a\x
        gen_sections128 b\x
.endm

.macro gen_sections512 x
        gen_sections256 a\x
        gen_sections256 b\x
.endm

.macro gen_sections1024 x
        gen_sections512 a\x
        gen_sections512 b\x
.endm

.macro gen_sections2048 x
        gen_sections1024 a\x
        gen_sections1024 b\x
.endm

.macro gen_sections4096 x
        gen_sections2048 a\x
        gen_sections2048 b\x
.endm

.macro gen_sections8192 x
        gen_sections4096 a\x
        gen_sections4096 b\x
.endm

.macro gen_sections16384 x
        gen_sections8192 a\x
        gen_sections8192 b\x
.endm

.macro gen_sections32768 x
        gen_sections16384 a\x
        gen_sections16384 b\x
.endm

gen_sections32768 a
gen_sections16384 b
gen_sections8192 c
gen_sections4096 d
gen_sections2048 e
gen_sections1024 f
gen_sections512 g
gen_sections128 h
gen_sections64 i
gen_sections32 j
gen_sections16 k
gen_sections8 l
gen_sections4 m
