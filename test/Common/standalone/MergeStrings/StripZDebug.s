## Checks reading of a bad merge string section with and without --strip-debug.
## With the flag, the section should not be processed at all, and omitted
## from the output. Without the flag, we should process and error as usual.

# RUN: %clang -c %clangopts %s -o %t.o

# RUN: %not %link %linkopts %t.o -o %t.bad.out 2>&1 | %filecheck %s --check-prefix=ERR
# RUN: %link %linkopts --strip-debug %t.o -o %t.strip.out
# RUN: %readelf -S %t.strip.out | %filecheck %s --check-prefix=STRIP

# ERR: Error: {{.*}}.o:(.zdebug_str): string is not null terminated
# STRIP-NOT: zdebug_str

.section .zdebug_str, "MS", %progbits,1
.ascii "unterminated"