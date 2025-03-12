volatile int data = 1;
volatile const int rodata = 0;
volatile int bss;

int main() {
  return data + rodata + bss;
}
