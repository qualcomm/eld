int j;
__thread int a;
__thread int b __attribute((tls_model ("local-exec")));
__thread int c __attribute((tls_model ("initial-exec")));
__thread int d __attribute((tls_model ("local-dynamic")));
__thread int e __attribute((tls_model ("global-dynamic")));

int
main (void)
{
  return a + b + c + d + e + j;
}
