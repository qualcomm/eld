; REQUIRES: aarch64
; RUN: %opt -o %t.o %s

; RUN: %link %linkopts -o %t1 -e main --plugin-opt=O1 %t.o
; RUN: llvm-objdump -d %t1 | FileCheck --check-prefix=CHECK-O1 %s
; RUN: %link %linkopts -o %t2 -e main --plugin-opt=O2 %t.o
; RUN: llvm-objdump -d %t2 | FileCheck --check-prefix=CHECK-O2 %s

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "aarch64-unknown-linux-gnu"

; CHECK-O1: ldrb
; CHECK-O2: add v{{.*}}.16b
define void @main(ptr nofree noundef captures(none) %p) local_unnamed_addr #0 {
entry:
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body
  ret void

for.body:                                         ; preds = %entry, %for.body
  %indvars.iv = phi i64 [ 0, %entry ], [ %indvars.iv.next, %for.body ]
  %arrayidx = getelementptr inbounds nuw i8, ptr %p, i64 %indvars.iv
  %0 = load i8, ptr %arrayidx, align 1
  %inc = add i8 %0, 1
  store i8 %inc, ptr %arrayidx, align 1
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, 1024
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body
}
