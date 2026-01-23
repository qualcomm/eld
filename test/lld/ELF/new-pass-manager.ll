; RUN: %opt --mtriple=%triple --data-layout=%datalayout %s -o %t.o

; Test debug-pass-manager option
; RUN: %link %linkopts --plugin-opt=debug-pass-manager -o %t.temp %t.o 2>&1 | FileCheck %s
; RUN: %link %linkopts --lto-debug-pass-manager -o %t.temp %t.o 2>&1 | FileCheck %s

; CHECK: Running pass: GlobalOptPass

define void @foo() {
  ret void
}
