#!/usr/bin/env bash
# Builds the Hexagon compiler-rt builtins that picolibc links against and
# installs them where the Hexagon driver searches for picolibc libraries:
#
#   <toolchain>/target/picolibc/hexagon-unknown-none-elf/lib/<cpu>[-G0][-pic]/
#
# For v68 this installs three archives:
#   v68         PIC off                 static links
#   v68-G0      -G0, PIC off            static links with -G0
#   v68-G0-pic  -G0 -fPIC, PIC on       -shared and -fPIC links (libc.so)
#
# ld.eld rejects the R_HEX_GPREL16_* relocations in the non-G0 archive when
# linking a shared object, so libc.so needs v68-G0-pic.
#
# Usage: BuildHexagonPicolibcBuiltins.sh <toolchain-install> <llvm-project-dir>

set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <toolchain-install> <llvm-project-dir>" >&2
  exit 2
fi

TOOLCHAIN_INSTALL=$1
LLVM_PROJECT=$2
TRIPLE=hexagon-unknown-none-elf
CPU=v68
SRC=$LLVM_PROJECT/compiler-rt
LIB_DIR=$TOOLCHAIN_INSTALL/target/picolibc/$TRIPLE/lib
CLANG=$(command -v clang)
CLANGXX=$(command -v clang++)
AR=$(command -v llvm-ar)
RANLIB=$(command -v llvm-ranlib)
WORK=$(mktemp -d)
COMMON_FLAGS="-ffreestanding -m$CPU -O3 -ffunction-sections -fdata-sections"

run_logged() {
  local log=$1
  shift
  if ! "$@" > "$log" 2>&1; then
    tail -n 60 "$log" >&2
    echo "error: $1 failed, full log in $log" >&2
    exit 1
  fi
}

build_variant() {
  local name=$1 flags=$2 pic=$3
  local build=$WORK/$name lib

  echo "==> $name: $flags (PIC $pic)"
  # Don't look for an installed LLVM: a system LLVM on the runner breaks Generic builds.
  run_logged "$WORK/$name.configure.log" cmake -S "$SRC" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$CLANG" \
    -DCMAKE_CXX_COMPILER="$CLANGXX" \
    -DCMAKE_ASM_COMPILER="$CLANG" \
    -DCMAKE_AR="$AR" \
    -DCMAKE_RANLIB="$RANLIB" \
    -DCMAKE_C_COMPILER_TARGET="$TRIPLE" \
    -DCMAKE_C_FLAGS="--target=$TRIPLE $flags" \
    -DCMAKE_ASM_FLAGS="--target=$TRIPLE $flags -mlong-calls" \
    -DCMAKE_CXX_FLAGS="--target=$TRIPLE -ffreestanding -nostdinc++" \
    -DCMAKE_C_COMPILER_FORCED=ON \
    -DCMAKE_CXX_COMPILER_FORCED=ON \
    -DCMAKE_CROSSCOMPILING=ON \
    -DCMAKE_SYSTEM_NAME=Generic \
    -DCOMPILER_RT_BAREMETAL_BUILD=ON \
    -DCOMPILER_RT_BUILTINS_ENABLE_PIC="$pic" \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
    -DCOMPILER_RT_ENABLE_WERROR=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=ON \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON
  run_logged "$WORK/$name.build.log" ninja -C "$build" clang_rt.builtins-hexagon

  # With LLVM_ENABLE_PER_TARGET_RUNTIME_DIR the archive is named
  # libclang_rt.builtins.a and placed under lib/<triple>.
  lib=$build/lib/$TRIPLE/libclang_rt.builtins.a
  if [[ ! -f $lib ]]; then
    echo "error: $name: $lib was not produced" >&2
    exit 1
  fi
  mkdir -p "$LIB_DIR/$name"
  cp "$lib" "$LIB_DIR/$name/"
}

build_variant "$CPU"        "$COMMON_FLAGS"           OFF
build_variant "$CPU-G0"     "$COMMON_FLAGS -G0"       OFF
build_variant "$CPU-G0-pic" "$COMMON_FLAGS -G0 -fPIC" ON

rm -rf "$WORK"
find "$LIB_DIR" -name libclang_rt.builtins.a | sort
