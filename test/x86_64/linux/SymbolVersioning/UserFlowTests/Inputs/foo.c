#include <stdio.h>

/* ---- foo: single version, lives in FOO_1.0 ---- */
__asm__(".symver foo_impl, foo@@FOO_1.0");
void foo_impl(void) {
    puts("libfoo: foo  [FOO_1.0 - default]");
}

/* ---- bar: two versions ---- */

/* Old implementation -- bound to FOO_1.0 (non-default '@') */
__asm__(".symver bar_v1, bar@FOO_1.0");
void bar_v1(void) {
    puts("libfoo: bar  [FOO_1.0 - old, non-default]");
}

/* New implementation -- bound to FOO_2.0 (default '@@') */
__asm__(".symver bar_v2, bar@@FOO_2.0");
void bar_v2(void) {
    puts("libfoo: bar  [FOO_2.0 - new, default]");
}

/* ---- baz: only in FOO_1.0 ---- */
__asm__(".symver baz_impl, baz@@FOO_1.0");
void baz_impl(void) {
    puts("libfoo: baz  [FOO_1.0 - default]");
}

/* ---- far: new in FOO_2.0 ---- */
__asm__(".symver far_impl, far@@FOO_2.0");
void far_impl(void) {
    puts("libfoo: far  [FOO_2.0 - default]");
}

/* ---- car: new in FOO_2.0 ---- */
__asm__(".symver car_impl, car@@FOO_2.0");
void car_impl(void) {
    puts("libfoo: car  [FOO_2.0 - default]");
}