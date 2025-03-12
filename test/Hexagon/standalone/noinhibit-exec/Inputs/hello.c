
int a = 10;

int printf(const char *format, ...);

__attribute__((section(".nottext")))
int main() {
  printf("Hello World");
  return 0;
}
