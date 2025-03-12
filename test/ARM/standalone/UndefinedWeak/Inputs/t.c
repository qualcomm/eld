extern void JustLoop(void) __attribute__((weak));

void __attribute__((noreturn)) tzbsp_cold_init (void *info, void *sbl_info)
{
  JustLoop();
}
