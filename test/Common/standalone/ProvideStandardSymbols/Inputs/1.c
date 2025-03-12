extern int __ehdr_start;
int foo() {
  return &__ehdr_start;
}
