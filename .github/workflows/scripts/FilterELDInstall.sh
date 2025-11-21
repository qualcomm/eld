#!/usr/bin/env bash

set -e
INSTALL_DIR="$1"
FILTERED_INSTALL_DIR="$2"

mkdir -p ${FILTERED_INSTALL_DIR}
cd ${FILTERED_INSTALL_DIR}
rm -rf *
mkdir -p bin include lib tools/bin
BIN_DIR=${INSTALL_DIR}/bin
cp ${BIN_DIR}/ld.eld ${BIN_DIR}/hexagon-link ${BIN_DIR}/arm-link \
  ${BIN_DIR}/aarch64-link ${BIN_DIR}/riscv-link ${BIN_DIR}/x86-link \
  ${BIN_DIR}/hexagon-link bin
cp ${INSTALL_DIR}/lib/libLW* lib
cp -r ${INSTALL_DIR}/templates .
cp -r ${INSTALL_DIR}/include/ELD .
cp ${INSTALL_DIR}/tools/bin/YAMLMapParser.py tools/bin
