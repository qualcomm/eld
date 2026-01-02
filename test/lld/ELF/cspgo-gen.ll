; RUN: %opt --mtriple=%triple --data-layout=%datalayout %s -o %t.o
; RUN: %link %linkopts --lto-cs-profile-generate --lto-cs-profile-file=%t.default.profraw %t.o --lto-debug-pass-manager -o %t 2>&1 | FileCheck %s --implicit-check-not=PGOInstrumentation
; RUN: %link %linkopts --plugin-opt=cs-profile-generate --plugin-opt=cs-profile-path=%t.default.profraw --lto-debug-pass-manager %t.o -o %t 2>&1 | FileCheck %s --implicit-check-not=PGOInstrumentation

; CHECK: PGOInstrumentationGen

@__llvm_profile_runtime = global i32 0, align 4

define void @foo() {
entry:
  ret void
}
