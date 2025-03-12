#---CommonSymbolValue.test----------------------- Executable --------------------#

#BEGIN_COMMENT
# This tests that the common symbol value is overridden only if the symbol
# chosen is from that library or object file.
#END_COMMENT
#START_TEST
RUN: %clang -c %p/Inputs/1.c -o %t1.1.o -G0
RUN: %clang -c %p/Inputs/2.c -o %t1.2.o -fPIC
RUN: %link -shared %t1.2.o -o %t1.lib2.so
RUN: %link -Bdynamic %t1.1.o %t1.lib2.so -o %t2.out -G0
RUN: %readelf -S -W %t2.out | %filecheck %s

#CHECK:  .bss              NOBITS          {{[0-9a-f]+}} {{[0-9a-f]+}} {{[0-9a-f]+}} {{[0-9a-f]+}}  WA  {{[0-9a-f]+}}   {{[0-9a-f]+}} 256

#END_TEST
