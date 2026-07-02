/* Driver references both `bar` (unversioned) and `bar@V1` (versioned).
 * Both should resolve to the same definition under rules 2/3/4. */
__asm__(".symver bar_v1, bar@V1");
extern int bar(void);
extern int bar_v1(void);
int main(void) { return bar() + bar_v1(); }
