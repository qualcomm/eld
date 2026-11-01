static int preempt_local __attribute__((used)) = 1;
int preempt_global = 2;
int preempt_hidden __attribute__((visibility("hidden"))) = 3;
int preempt_protected __attribute__((visibility("protected"))) = 4;
int preempt_weak __attribute__((weak)) = 5;
extern int preempt_undefined_weak __attribute__((weak));
int *weak_reference = &preempt_undefined_weak;
void preempt_function(void) {}
int main(void) { return 0; }
