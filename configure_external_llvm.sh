#!/bin/bash

# Configuration script for building ELD with external LLVM

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  ./configure_external_llvm.sh <LLVM_RELEASE_DIR> <ELD_BUILD_DIR> [options] [-- <cmake-args...>]

Arguments:
  LLVM_RELEASE_DIR   Path to an installed LLVM "release" directory containing:
                     - bin/clang, bin/clang++, bin/llvm-tblgen
                     - lib/cmake/llvm (or lib64/cmake/llvm)
  ELD_BUILD_DIR      Build directory to configure/build ELD into (created if needed)

Options:
  --target <ninja-target>
      Build this ninja target after configure. Default: ld.eld
  --no-build
      Configure only; do not run ninja.
  --ccache-dir <path>
      Use this ccache directory (restored/reused across runs).
      Default: ${ELD_BUILD_DIR}/.ccache
  --no-ccache
      Disable ccache even if installed.
  -- <cmake-args...>
      Forward all following arguments directly to cmake.

Examples:
  ./configure_external_llvm.sh /path/to/llvm.rel.latest ./llvm-build-ext-clang
  ./configure_external_llvm.sh /opt/llvm-22.0.0 /tmp/eld-build
  ./configure_external_llvm.sh /opt/llvm-22.0.0 /tmp/eld-build --target all
  ./configure_external_llvm.sh /opt/llvm-22.0.0 /tmp/eld-build --no-build
  ./configure_external_llvm.sh /opt/llvm-22.0.0 /tmp/eld-build -- -DLLVM_ENABLE_ASSERTIONS=ON
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ $# -lt 2 ]]; then
  echo "error: expected at least 2 arguments, got $#." >&2
  echo "" >&2
  usage >&2
  exit 2
fi

LLVM_RELEASE_DIR="$1"
BUILD_DIR_ARG="$2"
shift 2

BUILD_AFTER_CONFIG=1
NINJA_TARGET="ld.eld"
USE_CCACHE=1
CCACHE_DIR_ARG=""
EXTRA_CMAKE_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --target)
      [[ $# -ge 2 ]] || { echo "error: missing value for --target" >&2; exit 2; }
      NINJA_TARGET="$2"
      shift 2
      ;;
    --no-build)
      BUILD_AFTER_CONFIG=0
      shift
      ;;
    --ccache-dir)
      [[ $# -ge 2 ]] || { echo "error: missing value for --ccache-dir" >&2; exit 2; }
      CCACHE_DIR_ARG="$2"
      shift 2
      ;;
    --no-ccache)
      USE_CCACHE=0
      shift
      ;;
    --)
      shift
      EXTRA_CMAKE_ARGS=("$@")
      break
      ;;
    *)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ ! -d "${LLVM_RELEASE_DIR}" ]]; then
  echo "error: LLVM release directory does not exist: ${LLVM_RELEASE_DIR}" >&2
  exit 2
fi

# Canonicalize input paths
EXTERNAL_LLVM_ROOT="$(cd "${LLVM_RELEASE_DIR}" && pwd)"
BUILD_DIR="$(mkdir -p "${BUILD_DIR_ARG}" && cd "${BUILD_DIR_ARG}" && pwd)"
CCACHE_DIR="${CCACHE_DIR_ARG:-${BUILD_DIR}/.ccache}"

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
echo "Build target: ${NINJA_TARGET}"
echo "Build after configure: $([[ ${BUILD_AFTER_CONFIG} -eq 1 ]] && echo yes || echo no)"
if [[ ${#EXTRA_CMAKE_ARGS[@]} -gt 0 ]]; then
  echo "Extra CMake args: ${EXTRA_CMAKE_ARGS[*]}"
fi
echo ""

CCACHE_CMAKE_ARGS=()
if [[ ${USE_CCACHE} -eq 1 ]] && command -v ccache >/dev/null 2>&1; then
  mkdir -p "${CCACHE_DIR}"
  echo "ccache: enabled"
  echo "ccache directory: ${CCACHE_DIR}"
  ccache --show-stats --dir="${CCACHE_DIR}" || true
  CCACHE_CMAKE_ARGS+=(
    -DCMAKE_C_COMPILER_LAUNCHER=ccache
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    -DLLVM_CCACHE_BUILD:BOOL=ON
    -DLLVM_CCACHE_DIR:STRING="${CCACHE_DIR}"
  )
else
  echo "ccache: disabled"
fi

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
  "${CCACHE_CMAKE_ARGS[@]}" \
  "${EXTRA_CMAKE_ARGS[@]}" \
  "${SOURCE_DIR}"

echo ""
echo "=== Configuration complete ==="
if [[ ${BUILD_AFTER_CONFIG} -eq 1 ]]; then
  echo "Building target: ${NINJA_TARGET}"
  ninja "${NINJA_TARGET}"
  if [[ ${USE_CCACHE} -eq 1 ]] && command -v ccache >/dev/null 2>&1; then
    echo ""
    echo "ccache stats after build:"
    ccache --show-stats --dir="${CCACHE_DIR}" || true
  fi
else
  echo "To build, run:"
  echo "  cd ${BUILD_DIR}"
  echo "  ninja ${NINJA_TARGET}"
fi
