#include <stdio.h>

/* Symbols from libfoo (default versions resolved by the dynamic linker) */
extern void foo(void);
extern void bar(void);   /* will bind to bar@@FOO_2.0 -- the default */
extern void far(void);
extern void car(void);

/* Symbol from libbar */
extern void bar_wrapper(void);

int main(void) {
    puts("=== main: calling libfoo symbols (default versions) ===");

    /* foo lives only in FOO_1.0, so this calls foo@@FOO_1.0 */
    foo();

    /* bar has two versions; no annotation here means we get bar@@FOO_2.0 */
    bar();

    /* far and car are only in FOO_2.0 */
    far();
    car();

    puts("\n=== main: calling libbar (which uses bar@FOO_1.0 internally) ===");

    /* bar_wrapper is libbar's exported function; it calls bar@FOO_1.0 */
    bar_wrapper();

    puts("\nDone.");
    return 0;
}