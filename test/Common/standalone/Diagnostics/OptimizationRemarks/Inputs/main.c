
extern void bar(int *A, int *B, int N);
extern void baz(int * A, int i, int N);

int main()  {

  int A[1000];
  int B[1000];
  int N = 5;

  bar(A,B,5);
  baz(A,3,5);


  return 0;
}
