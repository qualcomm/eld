.section	foo
.cfi_startproc
nop
.cfi_endproc

.section	bar,"axG",@progbits,foo
.cfi_startproc
nop
nop
.cfi_endproc
