; REQUIRES: asserts

; RUN: %opt --mtriple=%triple --data-layout=%datalayout -o %t.bc %s

;; Try to save statistics to file.
; RUN: %link %linkopts --plugin-opt=stats-file=%t.stats -r -o %t.o %t.bc
; RUN: FileCheck --input-file=%t.stats %s

; CHECK: {
; CHECK: "asm-printer.EmittedInsts":
; CHECK: "inline.NumInlined":
; CHECK: "prologepilog.NumFuncSeen":
; CHECK: }

declare i32 @patatino()

define i32 @tinkywinky() {
  %a = call i32 @patatino()
  ret i32 %a
}

define i32 @main() !prof !0 {
  %i = call i32 @tinkywinky()
  ret i32 %i
}

!0 = !{!"function_entry_count", i64 300}
