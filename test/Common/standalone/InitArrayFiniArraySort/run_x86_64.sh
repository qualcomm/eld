#!/usr/bin/env bash
set -euo pipefail

CC=${CC:-clang}
LD_PATH=${LD_PATH:-""}
if [[ -z "${LD_PATH}" ]]; then
  LD_PATH=$(command -v ld.eld || true)
fi

if [[ -z "${LD_PATH}" ]]; then
  echo "error: ld.eld not found in PATH (set LD_PATH to override)" >&2
  exit 1
fi

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SRC_DIR="${SCRIPT_DIR}/Runtime"
BUILD_DIR=$(mktemp -d)
trap 'rm -rf "${BUILD_DIR}"' EXIT

"${CC}" -c "${SRC_DIR}/init_low.c" -o "${BUILD_DIR}/init_low.o"
"${CC}" -c "${SRC_DIR}/init_high.c" -o "${BUILD_DIR}/init_high.o"
"${CC}" -c "${SRC_DIR}/init_default.c" -o "${BUILD_DIR}/init_default.o"
"${CC}" -c "${SRC_DIR}/fini_low.c" -o "${BUILD_DIR}/fini_low.o"
"${CC}" -c "${SRC_DIR}/fini_high.c" -o "${BUILD_DIR}/fini_high.o"
"${CC}" -c "${SRC_DIR}/fini_default.c" -o "${BUILD_DIR}/fini_default.o"
"${CC}" -c "${SRC_DIR}/main.c" -o "${BUILD_DIR}/main.o"

"${CC}" -o "${BUILD_DIR}/init_fini_sort.exe" \
  "${BUILD_DIR}/init_low.o" \
  "${BUILD_DIR}/init_high.o" \
  "${BUILD_DIR}/init_default.o" \
  "${BUILD_DIR}/fini_low.o" \
  "${BUILD_DIR}/fini_high.o" \
  "${BUILD_DIR}/fini_default.o" \
  "${BUILD_DIR}/main.o" \
  --ld-path="${LD_PATH}"

OUTPUT=$("${BUILD_DIR}/init_fini_sort.exe")
EXPECTED="ABCMcba"

echo "Output: ${OUTPUT}"
if [[ "${OUTPUT}" != "${EXPECTED}" ]]; then
  echo "FAIL: expected '${EXPECTED}'" >&2
  exit 1
fi

echo "PASS"
