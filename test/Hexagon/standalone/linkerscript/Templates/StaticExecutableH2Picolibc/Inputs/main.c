/* Regression input for static-executable-h2-picolibc.lcs.template.
 *
 * References every symbol that the template defines via PROVIDE_HIDDEN or
 * PROVIDE so that those symbols are actually pulled into the output and can
 * be checked by the test.  Plain-assignment symbols (e.g. _TLS_START_,
 * __heap_start) are always emitted and do not need explicit references.
 *
 * PROVIDE_HIDDEN symbols referenced here:
 *   __preinit_array_start / __preinit_array_end
 *   __bothinit_array_start / __bothinit_array_end
 *   __init_array_start     / __init_array_end
 *   __fini_array_start     / __fini_array_end
 *
 * PROVIDE symbols referenced here:
 *   __etext / _etext / etext
 *   edata
 *   end
 *   _SDA_BASE_
 *   __sbss_start / ___sbss_start / __sbss_end / ___sbss_end
 */

/* TLS variables to populate .tdata and .tbss */
__thread int tdata_var = 42;
__thread int tbss_var;

/* --- PROVIDE_HIDDEN symbols --- */
extern char __preinit_array_start[];
extern char __preinit_array_end[];
extern char __bothinit_array_start[];
extern char __bothinit_array_end[];
extern char __init_array_start[];
extern char __init_array_end[];
extern char __fini_array_start[];
extern char __fini_array_end[];

/* --- PROVIDE symbols --- */
extern char __etext[];
extern char _etext[];
extern char etext[];
extern char edata[];
extern char end[];
extern char _SDA_BASE_[];
extern char __sbss_start[];
extern char ___sbss_start[];
extern char __sbss_end[];
extern char ___sbss_end[];

/* Volatile sink: prevents the compiler from optimising away the references. */
volatile char *sink;

int main(void) {
    /* Use TLS variables */
    tdata_var += tbss_var;

    /* Reference all PROVIDE_HIDDEN / PROVIDE symbols so the linker emits them */
    sink = __preinit_array_start;
    sink = __preinit_array_end;
    sink = __bothinit_array_start;
    sink = __bothinit_array_end;
    sink = __init_array_start;
    sink = __init_array_end;
    sink = __fini_array_start;
    sink = __fini_array_end;
    sink = __etext;
    sink = _etext;
    sink = etext;
    sink = edata;
    sink = end;
    sink = _SDA_BASE_;
    sink = __sbss_start;
    sink = ___sbss_start;
    sink = __sbss_end;
    sink = ___sbss_end;

    return tdata_var;
}
