#!/usr/bin/env bash

# Helpers for checking/fixing cpp-linter style issues on PR-changed files.
# Usage:
#   source etc/bash/eld_cpp_linter_helpers.sh
#   eld_cpp_linter_check [base-branch]
#   eld_cpp_linter_fix [base-branch]

eld_cpp_linter_usage() {
  cat <<'EOF'
Usage:
  eld_cpp_linter_check [--base-branch <branch>] [--build-directory <path>] [--file <path>]
  eld_cpp_linter_fix [--base-branch <branch>] [--build-directory <path>] [--file <path>]

Behavior:
  - --base-branch defaults to $BASE_BRANCH, or "main" if unset.
  - --build-directory is optional and should contain compile_commands.json.
  - --file limits check/fix to one explicit file.
  - Operates on files changed in: origin/<base-branch>...HEAD
  - Targets C/C++ sources: .c .cc .cpp .cxx .h .hh .hpp .hxx .inc .def
  - This helper only runs clang-tidy checks/fixes (not clang-format checks).
EOF
}

_eld_cpp_linter_changed_files() {
  local base_branch="$1"
  git fetch origin "$base_branch" >/dev/null 2>&1
  git diff --name-only --diff-filter=ACMRT "origin/${base_branch}...HEAD" |
    grep -E '\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inc|def)$' || true
}

_eld_cpp_linter_compile_db_dir() {
  local root="$1"
  local requested="${2:-}"
  if [[ -n "$requested" ]]; then
    if [[ -f "$requested/compile_commands.json" ]]; then
      echo "$requested"
      return 0
    fi
    if [[ -f "$root/$requested/compile_commands.json" ]]; then
      echo "$root/$requested"
      return 0
    fi
    return 1
  fi
  local d
  for d in "." "build" "llvm-build-ext-clang-debug"; do
    if [[ -f "$root/$d/compile_commands.json" ]]; then
      echo "$root/$d"
      return 0
    fi
  done
  return 1
}

_eld_cpp_linter_is_cpp_file() {
  local f="$1"
  [[ "$f" =~ \.(c|cc|cpp|cxx|h|hh|hpp|hxx|inc|def)$ ]]
}

_eld_cpp_linter_print_failure_details() {
  local tidy_out="$1"
  echo "    clang-tidy diagnostics:"
  sed 's/^/      /' "$tidy_out"
}

eld_cpp_linter_check() {
  local base_branch="${BASE_BRANCH:-main}"
  local requested_build_dir=""
  local requested_file=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -h|--help)
        eld_cpp_linter_usage
        return 0
        ;;
      --base-branch)
        [[ $# -ge 2 ]] || { echo "Missing value for --base-branch"; return 1; }
        base_branch="$2"
        shift 2
        ;;
      --build-directory)
        [[ $# -ge 2 ]] || { echo "Missing value for --build-directory"; return 1; }
        requested_build_dir="$2"
        shift 2
        ;;
      --file)
        [[ $# -ge 2 ]] || { echo "Missing value for --file"; return 1; }
        requested_file="$2"
        shift 2
        ;;
      *)
        echo "Unknown argument: $1"
        eld_cpp_linter_usage
        return 1
        ;;
    esac
  done
  local green=$'\033[0;32m'
  local red=$'\033[0;31m'
  local yellow=$'\033[0;33m'
  local reset=$'\033[0m'
  local failed=0
  local root
  local file_path
  root="$(git rev-parse --show-toplevel)" || return 1
  cd "$root" || return 1

  if [[ -n "$requested_file" ]]; then
    file_path="$requested_file"
    if [[ ! -f "$file_path" && -f "$root/$file_path" ]]; then
      file_path="$root/$file_path"
    fi
    if [[ ! -f "$file_path" ]]; then
      echo "Requested file not found: $requested_file"
      return 1
    fi
    if ! _eld_cpp_linter_is_cpp_file "$file_path"; then
      echo "Requested file is not a supported C/C++ source/header: $requested_file"
      return 1
    fi
    files=("$file_path")
  else
    mapfile -t files < <(_eld_cpp_linter_changed_files "$base_branch")
  fi
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "No changed C/C++ files to lint."
    return 0
  fi

  echo "Checking ${#files[@]} changed C/C++ file(s):"
  local f
  for f in "${files[@]}"; do
    echo "  - $f"
  done

  local tidy_bin=""
  command -v clang-tidy >/dev/null 2>&1 && tidy_bin="clang-tidy"

  local compile_db_dir=""
  if ! compile_db_dir="$(_eld_cpp_linter_compile_db_dir "$root" "$requested_build_dir")"; then
    if [[ -n "$requested_build_dir" ]]; then
      printf "%b[FAILED]%b build-dir does not contain compile_commands.json: %s\n" "$red" "$reset" "$requested_build_dir"
      return 1
    fi
    compile_db_dir=""
  fi
  if [[ -z "$tidy_bin" || -z "$compile_db_dir" ]]; then
    printf "%b[INFO]%b clang-tidy check skipped (missing clang-tidy or compile_commands.json).\n" "$yellow" "$reset"
  fi

  local tidy_out tidy_rc
  for f in "${files[@]}"; do
    [[ -f "$f" ]] || continue

    if [[ -n "$tidy_bin" && -n "$compile_db_dir" ]]; then
      tidy_out="$(mktemp)"
      tidy_rc=0
      "$tidy_bin" -p "$compile_db_dir" "$f" >"$tidy_out" 2>&1 || tidy_rc=$?
      if grep -Eq "no compile command|Error while processing" "$tidy_out"; then
        printf "%b[SKIPPED]%b clang-tidy  %s\n" "$yellow" "$reset" "$f"
      elif grep -Eq '(^|[^A-Za-z])warning:|(^|[^A-Za-z])error:' "$tidy_out"; then
        printf "%b[FAILED]%b clang-tidy  %s\n" "$red" "$reset" "$f"
        _eld_cpp_linter_print_failure_details "$tidy_out"
        failed=1
      elif [[ $tidy_rc -ne 0 ]]; then
        printf "%b[FAILED]%b clang-tidy  %s (exit %d)\n" "$red" "$reset" "$f" "$tidy_rc"
        _eld_cpp_linter_print_failure_details "$tidy_out"
        failed=1
      else
        printf "%b[PASSED]%b clang-tidy  %s\n" "$green" "$reset" "$f"
      fi
      rm -f "$tidy_out"
    fi
  done

  if [[ $failed -eq 0 ]]; then
    printf "%bcpp-linter check PASSED%b\n" "$green" "$reset"
  else
    printf "%bcpp-linter check FAILED%b\n" "$red" "$reset"
  fi
  return "$failed"
}

eld_cpp_linter_fix() {
  local base_branch="${BASE_BRANCH:-main}"
  local requested_build_dir=""
  local requested_file=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -h|--help)
        eld_cpp_linter_usage
        return 0
        ;;
      --base-branch)
        [[ $# -ge 2 ]] || { echo "Missing value for --base-branch"; return 1; }
        base_branch="$2"
        shift 2
        ;;
      --build-directory)
        [[ $# -ge 2 ]] || { echo "Missing value for --build-directory"; return 1; }
        requested_build_dir="$2"
        shift 2
        ;;
      --file)
        [[ $# -ge 2 ]] || { echo "Missing value for --file"; return 1; }
        requested_file="$2"
        shift 2
        ;;
      *)
        echo "Unknown argument: $1"
        eld_cpp_linter_usage
        return 1
        ;;
    esac
  done
  local green=$'\033[0;32m'
  local red=$'\033[0;31m'
  local yellow=$'\033[0;33m'
  local reset=$'\033[0m'
  local failed=0
  local root
  local file_path
  root="$(git rev-parse --show-toplevel)" || return 1
  cd "$root" || return 1

  if [[ -n "$requested_file" ]]; then
    file_path="$requested_file"
    if [[ ! -f "$file_path" && -f "$root/$file_path" ]]; then
      file_path="$root/$file_path"
    fi
    if [[ ! -f "$file_path" ]]; then
      echo "Requested file not found: $requested_file"
      return 1
    fi
    if ! _eld_cpp_linter_is_cpp_file "$file_path"; then
      echo "Requested file is not a supported C/C++ source/header: $requested_file"
      return 1
    fi
    files=("$file_path")
  else
    mapfile -t files < <(_eld_cpp_linter_changed_files "$base_branch")
  fi
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "No changed C/C++ files to fix."
    return 0
  fi

  echo "Fixing ${#files[@]} changed C/C++ file(s):"
  local f
  for f in "${files[@]}"; do
    echo "  - $f"
  done

  local tidy_bin=""
  command -v clang-tidy >/dev/null 2>&1 && tidy_bin="clang-tidy"

  local compile_db_dir=""
  if ! compile_db_dir="$(_eld_cpp_linter_compile_db_dir "$root" "$requested_build_dir")"; then
    if [[ -n "$requested_build_dir" ]]; then
      printf "%b[FAILED]%b build-dir does not contain compile_commands.json: %s\n" "$red" "$reset" "$requested_build_dir"
      return 1
    fi
    compile_db_dir=""
  fi

  if [[ -z "$tidy_bin" || -z "$compile_db_dir" ]]; then
    printf "%b[INFO]%b clang-tidy -fix skipped (missing clang-tidy or compile_commands.json).\n" "$yellow" "$reset"
  else
    local tidy_out tidy_rc
    for f in "${files[@]}"; do
      [[ -f "$f" ]] || continue
      tidy_out="$(mktemp)"
      tidy_rc=0
      "$tidy_bin" -p "$compile_db_dir" -fix -fix-errors "$f" >"$tidy_out" 2>&1 ||
          tidy_rc=$?
      if grep -Eq "no compile command|Error while processing" "$tidy_out"; then
        printf "%b[SKIPPED]%b   clang-tidy  %s\n" "$yellow" "$reset" "$f"
      elif [[ $tidy_rc -eq 0 ]]; then
        printf "%b[FIXED]%b     clang-tidy  %s\n" "$green" "$reset" "$f"
      else
        printf "%b[FAILED]%b    clang-tidy  %s (exit %d)\n" "$red" "$reset" "$f" "$tidy_rc"
        _eld_cpp_linter_print_failure_details "$tidy_out"
        failed=1
      fi
      rm -f "$tidy_out"
    done
  fi

  if [[ $failed -eq 0 ]]; then
    printf "%bcpp-linter fix PASSED%b\n" "$green" "$reset"
  else
    printf "%bcpp-linter fix FAILED%b\n" "$red" "$reset"
  fi
  return "$failed"
}
