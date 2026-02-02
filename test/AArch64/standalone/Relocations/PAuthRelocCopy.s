#---PAuthRelocCopy.s------------ Executable------------------#
#BEGIN_COMMENT
# Test that linker rejects AUTH relocations that would require COPY relocations.
#END_COMMENT
#START_TEST
# RUN: rm -rf %t && split-file %s %t && cd %t

# RUN: %llvm-mc -filetype=obj -triple=aarch64 a.s -o a.o
# RUN: %link %linkopts -shared a.o -soname=a.so -o a.so
# RUN: %llvm-mc -filetype=obj -triple=aarch64 main.s -o main.o

# Test non-PIE executable - this is where COPY relocations would be needed
# RUN: %not %link %linkopts main.o -no-pie a.so -o main 2>&1

# CHECK: {{.*}}relocation{{.*}}R_AARCH64_AUTH_ABS64{{.*}}cannot be used{{.*}}foo
# CHECK: {{.*}}relocation{{.*}}R_AARCH64_AUTH_ABS64{{.*}}cannot be used{{.*}}bar

# PIE executable should work (uses dynamic relocations, not COPY)
# RUN: %link %linkopts main.o -pie a.so -o main.pie
# RUN: %readelf --elf-output-style LLVM -r main.pie | %filecheck %s --check-prefix=PIE

# PIE: {{0x[0-9A-F]+}} R_AARCH64_AUTH_ABS64 bar 0x0
# PIE: {{0x[0-9A-F]+}} R_AARCH64_AUTH_ABS64 foo 0x0

#END_TEST

#--- a.s
.global bar
.type bar, @object
.size bar, 8
bar:
  .quad 0x1234

.global foo
.type foo, @object
.size foo, 4
foo:
  .word 42

#--- main.s
.global _start
_start:
  ret

.section .data
.p2align 3
.quad foo@AUTH(da,42)
.quad bar@AUTH(ia,43)
