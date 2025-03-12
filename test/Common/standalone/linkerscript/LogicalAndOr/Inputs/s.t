SECTIONS {
  foo = 1;
  ASSERT(foo == 1 || foo > 0, "foo is 1 or greater than 0");
  ASSERT(foo == 1 && foo > 0, "foo is 1 and  greater than 0");
  ASSERT(foo == 2 || foo > 0, "foo is 2 or greater than 0");
}
