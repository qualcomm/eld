extern unsigned char foo;
unsigned char *p = &foo;

int main() {
  return *p;
}
