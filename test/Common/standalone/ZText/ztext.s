## Checks that -z notext emits DT_TEXTREL and -z text does not.
# RUN: %clang %clangopts -c %s -o %t.o
# RUN: %link %linkopts -shared -z notext %t.o -o %t.notext.so
# RUN: %link %linkopts -shared -z text %t.o -o %t.text.so
# RUN: %readelf -d %t.notext.so | %filecheck %s --check-prefix=NOTEXT
# RUN: %readelf -d %t.text.so | %filecheck %s --check-prefix=TEXT --implicit-check-not=TEXTREL
# NOTEXT: TEXTREL
# TEXT: Dynamic section

.text
.globl foo
foo:
  .byte 0