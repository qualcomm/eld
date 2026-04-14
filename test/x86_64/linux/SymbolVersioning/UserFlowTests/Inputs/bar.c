#include <stdio.h>

/* Tell the linker: when this TU calls bar(), bind it to bar@FOO_1.0,
   NOT to the default bar@@FOO_2.0.  This is the canonical way for a
   library to pin itself to a specific symbol version from a dependency. */
__asm__(".symver bar, bar@FOO_1.0");

/* Forward-declare bar with the version we just pinned. */
extern void bar(void);

/* bar_wrapper is libbar's public API -- it delegates to the old bar. */
void bar_wrapper(void) {
    puts("libbar: bar_wrapper -> calling bar@FOO_1.0 from libfoo:");
    bar();
}