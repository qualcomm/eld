#!/bin/bash

# Configuration script for building ELD with external LLVM

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  ./configure_external_llvm.sh <LLVM_RELEASE_DIR> <ELD_BUILD_DIR>

Arguments:
  LLVM_RELEASE_DIR   Path to an installed LLVM "release" directory containing:
                     - bin/clang, bin/clang++, bin/llvm-tblgen
                     - lib/cmake/llvm (or lib64/cmake/llvm)
  ELD_BUILD_DIR      Build directory to configure/build ELD into (created if needed)

Examples:
  ./configure_external_llvm.sh /path/to/llvm.rel.latest ./llvm-build-ext-clang
  ./configure_external_llvm.sh /opt/llvm-22.0.0 /tmp/eld-build
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ $# -ne 2 ]]; then
  echo "error: expected 2 arguments, got $#." >&2
  echo "" >&2
  usage >&2
  exit 2
fi

LLVM_RELEASE_DIR="$1"
BUILD_DIR_ARG="$2"

if [[ ! -d "${LLVM_RELEASE_DIR}" ]]; then
  echo "error: LLVM release directory does not exist: ${LLVM_RELEASE_DIR}" >&2
  exit 2
fi

# Canonicalize input paths
EXTERNAL_LLVM_ROOT="$(cd "${LLVM_RELEASE_DIR}" && pwd)"
BUILD_DIR="$(mkdir -p "${BUILD_DIR_ARG}" && cd "${BUILD_DIR_ARG}" && pwd)"

EXTERNAL_LLVM_CMAKE="${EXTERNAL_LLVM_ROOT}/lib/cmake/llvm"
if [[ ! -d "${EXTERNAL_LLVM_CMAKE}" ]]; then
  alt="${EXTERNAL_LLVM_ROOT}/lib64/cmake/llvm"
  if [[ -d "${alt}" ]]; then
    EXTERNAL_LLVM_CMAKE="${alt}"
  fi
fi

if [[ ! -d "${EXTERNAL_LLVM_CMAKE}" ]]; then
  echo "error: could not find LLVM CMake package dir under ${EXTERNAL_LLVM_ROOT}." >&2
  echo "expected: lib/cmake/llvm or lib64/cmake/llvm" >&2
  exit 2
fi

if [[ ! -x "${EXTERNAL_LLVM_ROOT}/bin/clang" ]]; then
  echo "error: clang not found/executable: ${EXTERNAL_LLVM_ROOT}/bin/clang" >&2
  exit 2
fi

if [[ ! -x "${EXTERNAL_LLVM_ROOT}/bin/clang++" ]]; then
  echo "error: clang++ not found/executable: ${EXTERNAL_LLVM_ROOT}/bin/clang++" >&2
  exit 2
fi

if [[ ! -x "${EXTERNAL_LLVM_ROOT}/bin/llvm-tblgen" ]]; then
  echo "error: llvm-tblgen not found/executable: ${EXTERNAL_LLVM_ROOT}/bin/llvm-tblgen" >&2
  exit 2
fi

# Source directory (ELD)
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== ELD External LLVM Build Configuration ==="
echo "Source directory: ${SOURCE_DIR}"
echo "Build directory: ${BUILD_DIR}"
echo "External LLVM: ${EXTERNAL_LLVM_ROOT}"
echo "LLVM CMake dir: ${EXTERNAL_LLVM_CMAKE}"
echo ""

# Create build directory (already created during canonicalization)
cd "${BUILD_DIR}"

# Configure with CMake
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="${EXTERNAL_LLVM_ROOT}/bin/clang" \
  -DCMAKE_CXX_COMPILER="${EXTERNAL_LLVM_ROOT}/bin/clang++" \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++" \
  -DLLVM_DIR="${EXTERNAL_LLVM_CMAKE}" \
  -DLLVM_ENABLE_LLD:BOOL=ON \
  -DELD_USE_EXTERNAL_LLVM=ON \
  -DELD_TARGETS_TO_BUILD="Hexagon;AArch64;ARM;RISCV;X86" \
  -DLLVM_TABLEGEN_EXE="${EXTERNAL_LLVM_ROOT}/bin/llvm-tblgen" \
  -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLLVM_ENABLE_SPHINX=ON \
  "${SOURCE_DIR}"

echo ""
echo "=== Configuration complete ==="
echo "To build, run:"
echo "  cd ${BUILD_DIR}"
echo "  ninja"
echo ""
echo "To build specific targets:"
echo "  ninja ld.eld"
echo ""
