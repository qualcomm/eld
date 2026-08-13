/* main.c: calls add() which will be intercepted by --wrap=add */
extern int add(int a, int b);
int main() {
  int sum = add(40, 2);
  return sum;
}