
int main ()
{
  return callfar1() + callfar2();
}

int callfar1() {
  return far();
}

int callfar2() {
  return far();
}

int far() {
  return 0;
}
