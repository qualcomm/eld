	.option         nopic
#	.attribute      4, 16
#	.attribute      5, "rv32i2p1_m2p0_c2p0"

	.text
	.p2align        1

	lui     a0,%hi(A)
	lw      a0,%lo(A)(a0)
	lui     a0,%hi(B)
	lw      a0,%lo(B)(a0)
	lui     a0,%hi(C)
	lw      a0,%lo(C)(a0)
	lui     a0,%hi(D)
	lw      a0,%lo(D)(a0)
	lui     a0,%hi(E)
	lw      a0,%lo(E)(a0)
	lui     a0,%hi(F)
	lw      a0,%lo(F)(a0)
	lui     a0,%hi(G)
	lw      a0,%lo(G)(a0)
	lui     a0,%hi(H)
	lw      a0,%lo(H)(a0)
	lui     a0,%hi(I)
	lw      a0,%lo(I)(a0)
	lui     a0,%hi(J)
	lw      a0,%lo(J)(a0)
	lui     a0,%hi(K)
	lw      a0,%lo(K)(a0)
	lui     a0,%hi(L)
	lw      a0,%lo(L)(a0)
	lui     a0,%hi(M)
	lw      a0,%lo(M)(a0)
	lui     a0,%hi(N)
	lw      a0,%lo(N)(a0)
	lui     a0,%hi(O)
	lw      a0,%lo(O)(a0)
	lui     a0,%hi(P)
	lw      a0,%lo(P)(a0)
	lui     a0,%hi(Q)
	lw      a0,%lo(Q)(a0)
	lui     a0,%hi(R)
	lw      a0,%lo(R)(a0)
	lui     a0,%hi(S)
	lw      a0,%lo(S)(a0)
	lui     a0,%hi(T)
	lw      a0,%lo(T)(a0)
	lui     a0,%hi(U)
	lw      a0,%lo(U)(a0)

	# using x0, no relaxation
	lui     x0,%hi(D)
	lw      a0,%lo(D)(x0)

	# using x2, no relaxation
	lui     x2,%hi(D)
	lw      a0,%lo(D)(x2)
