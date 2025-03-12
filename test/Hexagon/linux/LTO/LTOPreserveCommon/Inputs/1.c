int a[100];
int main() {
  return a;
}

int bar(int b) {
  a[0] = b;
  return a[0]+b;
}
