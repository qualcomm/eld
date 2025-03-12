
void bar(int *A, int *B, int N) {
  for (int i = 0; i < N; ++i) {
    B[i] = i * N;
    A[i] += B[i];
  }
}

void baz(int * A, int i, int N) {

  while (A[i] == N) {
     A[i]++;
  }

}
