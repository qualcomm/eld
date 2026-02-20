#REQUIRES: x86, musl
#!/usr/bin/env bash
set -euo pipefail
SRC_DIR="%p/Runtime"
BUILD_DIR=%t
%mkdir -p $BUILD_DIR

%musl-clang %clangopts -c "${SRC_DIR}/init_low.c" -o "${BUILD_DIR}/init_low.o"
%musl-clang %clangopts -c "${SRC_DIR}/init_high.c" -o "${BUILD_DIR}/init_high.o"
%musl-clang %clangopts -c "${SRC_DIR}/init_default.c" -o "${BUILD_DIR}/init_default.o"
%musl-clang %clangopts -c "${SRC_DIR}/fini_low.c" -o "${BUILD_DIR}/fini_low.o"
%musl-clang %clangopts -c "${SRC_DIR}/fini_high.c" -o "${BUILD_DIR}/fini_high.o"
%musl-clang %clangopts -c "${SRC_DIR}/fini_default.c" -o "${BUILD_DIR}/fini_default.o"
%musl-clang %clangopts -c "${SRC_DIR}/main.c" -o "${BUILD_DIR}/main.o"

%musl-clang %clangopts -o "${BUILD_DIR}/init_fini_sort.exe" \
  "${BUILD_DIR}/init_low.o" \
  "${BUILD_DIR}/init_high.o" \
  "${BUILD_DIR}/init_default.o" \
  "${BUILD_DIR}/fini_low.o" \
  "${BUILD_DIR}/fini_high.o" \
  "${BUILD_DIR}/fini_default.o" \
  "${BUILD_DIR}/main.o"

%run ${BUILD_DIR}/init_fini_sort.exe 2>&1 | %filecheck %s
#CHECK: ABCMcba
