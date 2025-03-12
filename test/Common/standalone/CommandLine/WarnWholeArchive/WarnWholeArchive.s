//---WarnWholeArchive.s--------------------------- Executable --------------------#

//BEGIN_COMMENT
// This checks for -Wwhole-archive option to warn when whole-archive is enabled
// and is shown only once when lto is enabled.
//END_COMMENT
//START_TEST

foo:
  nop

// RUN: %clang %clangopts -c %s -o %t.o
// RUN: %ar cr %aropts %t1.lib1.a %t.o
// RUN: %link %linkopts -whole-archive %t1.lib1.a -o %t2.out -Wwhole-archive \
// RUN: 2>&1 | %filecheck %s --check-prefix=WARN1
// WARN1: Warning: 'whole-archive' enabled for{{.*}}lib1.a
// RUN: %link %linkopts %t1.lib1.a -o %t2.out -Wwhole-archive --verbose \
// RUN: 2>&1 | %filecheck %s --check-prefix=WARN2
// WARN2-NOT: Warning: 'whole-archive' enabled for{{.*}}lib1.a
// RUN: %clang %clangopts -c %s -flto -o %t.lto.o
// RUN: %ar cr %aropts %t1.ltolib1.a %t.lto.o
// RUN: %link %linkopts -whole-archive %t1.ltolib1.a -o %t2.ltoout -Wwhole-archive \
// RUN: 2>&1 | %filecheck %s --check-prefix=WARN3
// WARN3-COUNT-1: Warning: 'whole-archive' enabled for{{.*}}ltolib1.a
//END_TEST
