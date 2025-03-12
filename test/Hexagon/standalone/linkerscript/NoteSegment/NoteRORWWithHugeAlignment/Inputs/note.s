	.section	".note.ro","a",@note
        .global foo
foo:
        .word 0
	.section	".data","aw",@progbits
        .global baz
baz:
        nop
	.section	".note.rw","aw",@note
        .global bar
bar:
        .word 0
	.section	".anotherdata","aw",@progbits
        .global anotherdata
anotherdata:
        .word 0
	.section	".note.another.ro","a",@note
        .p2align 18
        .global anotherfoo
anotherfoo:
        .word 0
