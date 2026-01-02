; RUN: %opt --mtriple=%triple --data-layout=%datalayout %s -o %t.o
; RUN: %link %linkopts %t.o -o %t2 --lto-debug-pass-manager \
; RUN:   2>&1 | FileCheck -check-prefix=DEFAULT-NPM %s
; RUN: %link %linkopts %t.o -o %t2 --lto-debug-pass-manager \
; RUN:   -disable-verify 2>&1 | FileCheck -check-prefix=DISABLE-NPM %s
; RUN: %link %linkopts %t.o -o %t2 --lto-debug-pass-manager \
; RUN:   --plugin-opt=disable-verify 2>&1 | FileCheck -check-prefix=DISABLE-NPM %s

define void @_start() {
  ret void
}

; -disable-verify should disable the verification of bitcode.
; DEFAULT-NPM: Running pass: VerifierPass
; DEFAULT-NPM: Running pass: VerifierPass
; DEFAULT-NPM-NOT: Running pass: VerifierPass
; DISABLE-NPM-NOT: Running pass: VerifierPass
