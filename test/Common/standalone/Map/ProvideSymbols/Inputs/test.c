extern char *__text_start;
extern char *__text_end;
extern char *__text_size;

int main(int argc, char *argv[]) {
  char *start = __text_start;
  char *end = __text_end;
  char *size = __text_size;
}
