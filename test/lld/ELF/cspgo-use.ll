; Create an empty profile
; RUN: %echo > %t.proftext
; RUN: llvm-profdata merge %t.proftext -o %t.profdata

; RUN: %opt --mtriple=%triple --data-layout=%datalayout %s -o %t.o
; RUN: %link %linkopts --lto-cs-profile-file=%t.profdata %t.o --lto-debug-pass-manager  -o %t 2>&1 | FileCheck %s --implicit-check-not=PGOInstrumentation
; RUN: %link %linkopts --plugin-opt=cs-profile-path=%t.profdata --lto-debug-pass-manager %t.o -o %t 2>&1 | FileCheck %s --implicit-check-not=PGOInstrumentation

; CHECK: Running pass: PGOInstrumentationUse

define void @foo() {
entry:
  ret void
}
