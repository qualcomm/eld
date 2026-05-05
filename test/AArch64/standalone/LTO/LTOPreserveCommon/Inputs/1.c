int a[100];
int main() {
  return a[0];
}

int bar(int b) {
  a[0] = b;
  return a[0]+b;
}
