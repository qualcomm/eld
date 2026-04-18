#!/usr/bin/env bash

# Helpers for checking/fixing clang-format on PR-changed files.
# Usage:
#   source etc/bash/eld_clang_format_helpers.sh
#   eld_clang_format_check [base-branch]
#   eld_clang_format_fix [base-branch]

eld_clang_format_usage() {
  cat <<'EOF'
Usage:
  eld_clang_format_check [base-branch]
  eld_clang_format_fix [base-branch]

Behavior:
  - base-branch defaults to $BASE_BRANCH, or "main" if unset.
  - Operates on files changed in: origin/<base-branch>...HEAD
  - Checks/formats C/C++ sources: .c .cc .cpp .cxx .h .hh .hpp .hxx .inc .def
EOF
}

eld_clang_format_check() {
  if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    eld_clang_format_usage
    return 0
  fi
  local base_branch="${1:-${BASE_BRANCH:-main}}"
  local failed=0
  local green=$'\033[0;32m'
  local red=$'\033[0;31m'
  local reset=$'\033[0m'
  git fetch origin "$base_branch" || return 1
  while IFS= read -r f; do
    [ -f "$f" ] || continue
    if ! clang-format --style=file "$f" | diff -u "$f" - >/dev/null; then
      printf "%b[FAILED]%b %s\n" "$red" "$reset" "$f"
      failed=1
    else
      printf "%b[PASSED]%b %s\n" "$green" "$reset" "$f"
    fi
  done < <(
    git diff --name-only --diff-filter=ACMRT "origin/${base_branch}...HEAD" |
      grep -E '\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inc|def)$' || true
  )
  if [[ $failed -eq 0 ]]; then
    printf "%bclang-format check PASSED%b\n" "$green" "$reset"
  else
    printf "%bclang-format check FAILED%b\n" "$red" "$reset"
  fi
  return "$failed"
}

eld_clang_format_fix() {
  if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    eld_clang_format_usage
    return 0
  fi
  local base_branch="${1:-${BASE_BRANCH:-main}}"
  local green=$'\033[0;32m'
  local red=$'\033[0;31m'
  local reset=$'\033[0m'
  local failed=0
  git fetch origin "$base_branch" || return 1
  while IFS= read -r f; do
    [ -f "$f" ] || continue
    if clang-format --style=file -i "$f"; then
      printf "%b[FORMATTED]%b %s\n" "$green" "$reset" "$f"
    else
      printf "%b[FAILED]%b %s\n" "$red" "$reset" "$f"
      failed=1
    fi
  done < <(
    git diff --name-only --diff-filter=ACMRT "origin/${base_branch}...HEAD" |
      grep -E '\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inc|def)$' || true
  )
  return "$failed"
}
