	.option         nopic
	.attribute      4, 16
	.attribute      5, "rv32i2p1_m2p0_c2p0"

	.text
	.org   0x0
	.p2align        1
foo:
	addi    a0, a0, 1
	ret

	tail    foo

	.org    0x802
	tail    foo

	.org    0x1002
	tail    foo

