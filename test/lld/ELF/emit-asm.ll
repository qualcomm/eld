; REQUIRES: x86

; RUN: %rm %t.*
; RUN: %llvm-as %s -o %t.bc

; RUN: %link %linkopts %emulation --lto-emit-asm -shared %t.bc -o %t.out
; RUN: FileCheck %s < %t.out.llvm-lto.0.s
; RUN: not ls %t.out.*.o

; RUN: %link %linkopts %emulation --plugin-opt=emit-asm --plugin-opt=lto-partitions=2 -shared %t.bc -o %t.out
; RUN: cat %t.out.llvm-lto.0.s %t.out.llvm-lto.1.s | FileCheck %s
; RUN: not ls %t.out.*.o

; RUN: %link %linkopts --lto-emit-asm --save-temps -shared %t.bc -o %t.out
; RUN: FileCheck --input-file %t.out.llvm-lto.0.s %s
; RUN: not ls %t.out.*.o
; RUN: %llvm-dis %t.out.llvm-lto.0.4.opt.bc -o - | FileCheck --check-prefix=OPT %s

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

;; Note: we also check for the presence of comments; --lto-emit-asm output should be verbose.

; CHECK-DAG: # -- Begin function f1
; CHECK-DAG: f1:
; OPT: define void @f1()
define void @f1() {
  ret void
}

; CHECK-DAG: # -- Begin function f2
; CHECK-DAG: f2:
; OPT: define void @f2()
define void @f2() {
  ret void
}
