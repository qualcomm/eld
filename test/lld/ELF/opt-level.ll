; RUN: %opt --mtriple=%triple --data-layout=%datalayout -o %t.o %s

; RUN: %link %linkopts -o %t0 -e main --lto-O0 %t.o
; RUN: llvm-nm %t0 | FileCheck --check-prefix=CHECK-O0 %s
; RUN: %link %linkopts -o %t0 -e main --plugin-opt=O0 %t.o
; RUN: llvm-nm %t0 | FileCheck --check-prefix=CHECK-O0 %s
; RUN: %link %linkopts -o %t2 -e main --lto-O2 %t.o
; RUN: llvm-nm %t2 | FileCheck --check-prefix=CHECK-O2 %s
; RUN: %link %linkopts -o %t2a -e main %t.o
; RUN: llvm-nm %t2a | FileCheck --check-prefix=CHECK-O2 %s
; RUN: %link %linkopts -o %t2 -e main %t.o --plugin-opt O2
; RUN: llvm-nm %t2 | FileCheck --check-prefix=CHECK-O2 %s

; Reject invalid optimization levels.
; RUN: not %link %linkopts -o /dev/null -e main --lto-O6 %t.o 2>&1 | \
; RUN:   FileCheck --check-prefix=INVALID1 %s
; INVALID1: Error: Invalid value for --lto-O: 6
; RUN: not %link %linkopts -o /dev/null -e main --plugin-opt=O6 %t.o 2>&1 | \
; RUN:   FileCheck --check-prefix=INVALID1 %s
; RUN: not %link %linkopts -o /dev/null -e main --plugin-opt=Ofoo %t.o 2>&1 | \
; RUN:   FileCheck --check-prefix=INVALID2 %s
; INVALID2: Error: Invalid value for --lto-O: foo

; RUN: not %link %linkopts -o /dev/null -e main --lto-O-1 %t.o 2>&1 | \
; RUN:   FileCheck --check-prefix=INVALIDNEGATIVE1 %s
; INVALIDNEGATIVE1: Error: Invalid value for --lto-O: -1
; RUN: not %link %linkopts -o /dev/null -e main --plugin-opt=O-1 %t.o 2>&1 | \
; RUN:   FileCheck --check-prefix=INVALIDNEGATIVE2 %s
; INVALIDNEGATIVE2: Error: Invalid value for --lto-O: -1

; CHECK-O0: foo
; CHECK-O2-NOT: foo
define internal void @foo() #0 {
  ret void
}

define void @main() #0 {
  call void @foo()
  ret void
}

attributes #0 = { nounwind }
