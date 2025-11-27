extern int u;
extern int v;

int get_via_pointer(){
  int *p = &v;
  return *p;
}
int main() {
  u = 11;
  return u + get_via_pointer();
}