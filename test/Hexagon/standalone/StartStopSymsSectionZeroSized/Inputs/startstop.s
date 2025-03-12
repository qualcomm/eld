
	.section	zerosized,"aw",@progbits
	.section	bar,"ax",@progbits
        .global ban
ban:
        .word sym
        nop
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
	.section	startstop,"aw",@progbits
        .word __start_zerosized
        .word __stop_zerosized
        .global sym
sym:
        .word 0
