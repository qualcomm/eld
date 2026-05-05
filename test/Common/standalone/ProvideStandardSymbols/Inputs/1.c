extern int __ehdr_start;
int foo() {
  return (int)(long)(&__ehdr_start);
}
