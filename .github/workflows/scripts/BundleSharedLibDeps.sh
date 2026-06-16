#!/usr/bin/env bash
# Bundle shared library dependencies for a list of binaries into INSTALL_ROOT/lib.
# Only libraries whose basename matches a pattern in ALLOWED_LIBS are packaged.
# Usage: BundleSharedLibDeps.sh <install-root> <binary> [<binary> ...]

set -euo pipefail

# Allowlist of library name prefixes to package (fnmatch patterns against basename).
ALLOWED_LIBS=(
  "libc++.so*"
  "libc++abi.so*"
)

INSTALL_ROOT="$1"; shift
BIN_DIR="${INSTALL_ROOT}/bin"
LIB_DIR="${INSTALL_ROOT}/lib"
mkdir -p "${LIB_DIR}"

is_allowed() {
  local base="$1"
  for pattern in "${ALLOWED_LIBS[@]}"; do
    # shellcheck disable=SC2254
    case "${base}" in
      ${pattern}) return 0 ;;
    esac
  done
  return 1
}

for tool in "$@"; do
  if [[ -x "${BIN_DIR}/${tool}" ]]; then
    echo "Collecting shared libraries for ${tool}"
    ldd "${BIN_DIR}/${tool}" | awk '/=> \// { print $3 } /^\/.*\.so/ { print $1 }' | while read -r dep; do
      [[ -n "${dep}" && -f "${dep}" ]] || continue
      base="$(basename "${dep}")"
      if ! is_allowed "${base}"; then
        continue
      fi
      if compgen -G "${LIB_DIR}/${base}*" > /dev/null; then
        continue
      fi
      cp -P "${dep}" "${LIB_DIR}/"
      cp -n "$(readlink -f "${dep}")" "${LIB_DIR}/" || true
    done
  fi
done

echo "Packaged shared libraries in ${LIB_DIR}:"
ls -la "${LIB_DIR}"
