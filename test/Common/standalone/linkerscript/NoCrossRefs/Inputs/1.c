
int common;
extern int another_common;
extern int z;
int *x = &z;
int baz() { return z; }

int main() {
  common = 1;
  another_common = 1;
  return common + another_common + baz();
}
