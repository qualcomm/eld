extern int baz();

int bar() {
  return baz();
}

int baz() {
  return bar();
}

int main(int argc, char *argv[]) {
  return bar();
}
