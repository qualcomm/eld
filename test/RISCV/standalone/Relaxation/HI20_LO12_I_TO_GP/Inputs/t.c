int __attribute__((section(".sdata"))) near;
int __attribute__((section(".sdata"))) far[2];
extern int __global_pointer$;

int data(void) {
  return near | far[1];
}

int main() {
  int i = data();
  i += __global_pointer$;
  return i;
}
