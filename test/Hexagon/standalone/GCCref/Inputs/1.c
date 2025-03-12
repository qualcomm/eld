int commonInt;
char commonChar;
int commonArray[10];

void main() {
commonChar = 'a';
commonInt = foo() + bar();
}

int foo() {
  return bar();
}

int bar () {
  return 0;
}
