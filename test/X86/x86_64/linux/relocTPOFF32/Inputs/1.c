__thread int tls_var1 = 42;
__thread int tls_var2 = 0;

int main() {
  tls_var2 += 5;
  return tls_var1 + tls_var2;
}
