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
    (falls back to a local <base-branch> ref if origin/<base-branch> is unavailable)
  - Checks/formats C/C++ sources: .c .cc .cpp .cxx .h .hh .hpp .hxx .inc .def
EOF
}

eld_clang_format_resolve_base_ref() {
  local base_branch="$1"
  # Best effort: refresh the remote ref when available, but do not fail on
  # local-only workflows/offline environments.
  git fetch origin "$base_branch" >/dev/null 2>&1 || true

  # Prefer remote tracking branch for PR-equivalent behavior.
  if git rev-parse --verify --quiet "refs/remotes/origin/${base_branch}" >/dev/null; then
    echo "origin/${base_branch}"
    return 0
  fi
  if git rev-parse --verify --quiet "refs/heads/${base_branch}" >/dev/null; then
    echo "${base_branch}"
    return 0
  fi
  if git rev-parse --verify --quiet "${base_branch}" >/dev/null; then
    echo "${base_branch}"
    return 0
  fi
  return 1
}

eld_clang_format_diff_range() {
  local base_branch="$1"
  local base_ref
  if ! base_ref="$(eld_clang_format_resolve_base_ref "$base_branch")"; then
    echo "Unable to resolve base branch ref: ${base_branch}" >&2
    return 1
  fi
  echo "${base_ref}...HEAD"
}

eld_clang_format_changed_files() {
  local diff_range
  diff_range="$(eld_clang_format_diff_range "$1")" || return 1
  # Restrict to source-like files that are expected to follow clang-format.
  git diff --name-only --diff-filter=ACMRT "$diff_range" -- \
    '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' '*.hxx' '*.inc' '*.def'
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
  local base_ref
  local -a files=()
  local format_diff
  if ! git clang-format -h >/dev/null 2>&1; then
    echo "Required tool not found: git clang-format"
    return 1
  fi
  if ! base_ref="$(eld_clang_format_resolve_base_ref "$base_branch")"; then
    echo "Unable to resolve base branch ref: ${base_branch}" >&2
    return 1
  fi

  mapfile -t files < <(eld_clang_format_changed_files "$base_branch")
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "No C/C++ source files changed."
    return 0
  fi

  format_diff="$(
    git clang-format --diff "$base_ref" -- "${files[@]}"
  )" || {
    echo "Failed to run git clang-format --diff"
    return 1
  }

  # git clang-format may print informational text; only fail when there are
  # actual patch hunks.
  if grep -q '^diff --git ' <<<"$format_diff"; then
    echo "$format_diff"
    failed=1
  fi

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
  local base_ref
  local -a files=()
  local -a existing_files=()
  local -a missing_files=()
  local -a changed_files=()
  local -a unchanged_files=()
  local f
  local before_hash
  local after_hash
  declare -A file_hash_before=()

  if ! git clang-format -h >/dev/null 2>&1; then
    echo "Required tool not found: git clang-format"
    return 1
  fi
  if ! base_ref="$(eld_clang_format_resolve_base_ref "$base_branch")"; then
    echo "Unable to resolve base branch ref: ${base_branch}" >&2
    return 1
  fi

  mapfile -t files < <(eld_clang_format_changed_files "$base_branch")
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "No C/C++ source files changed."
    return 0
  fi

  for f in "${files[@]}"; do
    if [[ -f "$f" ]]; then
      existing_files+=("$f")
      # Cache pre-format hashes to report which files were modified.
      file_hash_before["$f"]="$(git hash-object -- "$f")"
    else
      missing_files+=("$f")
    fi
  done

  if [[ ${#existing_files[@]} -eq 0 ]]; then
    echo "No existing files to format."
    return 0
  fi

  echo "clang-format candidates (${#existing_files[@]} files):"
  for f in "${existing_files[@]}"; do
    printf "  %s\n" "$f"
  done
  for f in "${missing_files[@]}"; do
    printf "%b[SKIPPED]%b missing file: %s\n" "$red" "$reset" "$f"
  done

  # Format only lines that differ from base_ref, then report file-level impact.
  if git clang-format "$base_ref" -- "${existing_files[@]}"; then
    for f in "${existing_files[@]}"; do
      before_hash="${file_hash_before["$f"]}"
      after_hash="$(git hash-object -- "$f")"
      if [[ "$before_hash" != "$after_hash" ]]; then
        changed_files+=("$f")
      else
        unchanged_files+=("$f")
      fi
    done

    for f in "${changed_files[@]}"; do
      printf "%b[FORMATTED]%b %s\n" "$green" "$reset" "$f"
    done
    for f in "${unchanged_files[@]}"; do
      printf "[UNCHANGED] %s\n" "$f"
    done

    printf "%b[SUMMARY]%b formatted %d of %d candidate files\n" \
      "$green" "$reset" "${#changed_files[@]}" "${#existing_files[@]}"
    return 0
  fi

  printf "%b[FAILED]%b unable to apply clang-format to diff hunks\n" "$red" "$reset"
  return 1
}
