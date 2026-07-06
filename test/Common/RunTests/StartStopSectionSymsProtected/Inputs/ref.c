extern int __start_foobar;
extern int __stop_foobar;

long foobar_count(void) { return (long)(&__stop_foobar - &__start_foobar); }
