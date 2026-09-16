; ModuleID = 'perf_art.o'
source_filename = "scanner.no-prof.ll"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "aarch64-unknown-none-elf"

%struct.xyz = type { double, i32 }
%struct.f1_neuron = type { double*, double, double, double, double, double, double, double }

@win = common global i32 0, align 4
@Y = common global %struct.xyz* null, align 8
@d = common global double 0.000000e+00, align 8
@num2 = common global i32 0, align 4
@num1 = common global i32 0, align 4
@f1 = common global %struct.f1_neuron* null, align 8
@c = common global double 0.000000e+00, align 8
@t = common global double** null, align 8
@a = common global double 0.000000e+00, align 8
@b = common global double 0.000000e+00, align 8

define i32 @main(i32 %argc, i8** %argv) {
  ret i32 0
}
