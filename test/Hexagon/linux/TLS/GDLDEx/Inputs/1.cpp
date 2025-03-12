
static thread_local volatile int g_usr1_called;

void
SigUsr1Handler (int)
{
    g_usr1_called = 1;
}
extern void puts(char *);
bool
CheckForMonitorCancellation()
{

 puts("hello");
    if (g_usr1_called)
    {
        g_usr1_called = 0;
        return true;
    }
    return false;
}
int
main() {
  return 0;
}
