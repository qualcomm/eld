A = DEFINED(B);
C = DEFINED(A);
ASSERT(!A, "Undefined A");
ASSERT(C, "Defined C");
