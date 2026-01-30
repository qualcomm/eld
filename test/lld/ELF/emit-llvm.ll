; UNSUPPORTED: windows

; RUN: %rm %t.*
; RUN: %opt --mtriple=%triple --data-layout=%datalayout -module-hash -module-summary %s -o %t.o
; RUN: %link %linkopts %emulation --plugin-opt=emit-llvm -o %t.out.o %t.o
; RUN: %llvm-dis < %t.out.o -o - | FileCheck %s
; RUN: %not ls %t.out.*.o

;; Regression test for D112297: bitcode writer used to crash when
;; --plugin-opt=emit-llvm is enabled and the output is /dev/null.
; RUN: %link %linkopts %emulation --plugin-opt=emit-llvm -mllvm -bitcode-flush-threshold=0 -o /dev/null %t.o
; RUN: %link %linkopts %emulation --lto-emit-llvm -mllvm -bitcode-flush-threshold=0 -o /dev/null %t.o

; CHECK: define hidden void @main()

@llvm.compiler.used = appending global [1 x ptr] [ptr @main], section "llvm.metadata"

define hidden void @main() {
  ret void
}
