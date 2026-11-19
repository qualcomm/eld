#!/usr/bin/env bash
# ELD Linker Performance Benchmark
#
# Usage:
#   run_benchmark.sh --eld <path-to-ld.eld> --reproducers <path-to-reproducer-store>
#
# Output:
#   JSON array to stdout, one object per workload:
#   [{"workload":"firefox-x64","wall_seconds":68.5,"peak_memory_mb":14400,"exit_status":0}, ...]
#
# The reproducers directory should contain subdirectories (one per workload),
# each with a response.txt file. Compatible with the lld speed-test reproducer
# format from https://github.com/llvm/llvm-project/tree/main/lld/utils/speed-test-reproducers

set -euo pipefail

ELD_BINARY=""
REPRODUCERS_DIR=""

usage() {
  echo "Usage: $0 --eld <ld.eld> --reproducers <dir>" >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --eld)        ELD_BINARY="$2";      shift 2 ;;
    --reproducers) REPRODUCERS_DIR="$2"; shift 2 ;;
    *) usage ;;
  esac
done

[[ -z "$ELD_BINARY" || -z "$REPRODUCERS_DIR" ]] && usage
[[ ! -x "$ELD_BINARY" ]]    && { echo "Error: $ELD_BINARY not executable" >&2; exit 1; }
[[ ! -d "$REPRODUCERS_DIR" ]] && { echo "Error: $REPRODUCERS_DIR not a directory" >&2; exit 1; }

TMPDIR_BENCH="/tmp/eld-benchmark-work"
rm -rf "$TMPDIR_BENCH"
mkdir -p "$TMPDIR_BENCH"

# Apply fixes to response.txt so ELD can parse options that lld accepts
# but ELD does not yet recognize:
#   1. --compress-debug-sections zlib  -> --compress-debug-sections=zlib
#   2. --dependency-file <path>        -> (removed, two-line form)
fix_response() {
  local src="$1" dst="$2"
  cp "$src" "$dst"
  sed -i 's/--compress-debug-sections zlib/--compress-debug-sections=zlib/' "$dst"
  sed -i '/^--dependency-file/{N;d}' "$dst"
}


echo "["
first=1

for WORKLOAD_DIR in "$REPRODUCERS_DIR"/*/; do
  WORKLOAD=$(basename "$WORKLOAD_DIR")
  RESPONSE="$WORKLOAD_DIR/response.txt"

  if [[ ! -f "$RESPONSE" ]]; then
    echo "  SKIP $WORKLOAD (no response.txt)" >&2
    continue
  fi

  FIXED_RESPONSE="$TMPDIR_BENCH/response-${WORKLOAD}.txt"
  fix_response "$RESPONSE" "$FIXED_RESPONSE"

  OUTPUT_FILE="$TMPDIR_BENCH/${WORKLOAD}-out"
  TIME_LOG="$TMPDIR_BENCH/${WORKLOAD}-time.txt"
  ELD_LOG="$TMPDIR_BENCH/${WORKLOAD}-eld.txt"
  HYPERFINE_JSON="$TMPDIR_BENCH/${WORKLOAD}-hyperfine.json"

  echo "  Running $WORKLOAD ..." >&2

  cd "$WORKLOAD_DIR"

  # Use hyperfine for statistically sound wall-time measurement (1 warmup + 3 timed runs).
  EXIT_STATUS=0
  hyperfine --warmup 1 --runs 3 \
    --export-json "$HYPERFINE_JSON" \
    -- "$ELD_BINARY @$FIXED_RESPONSE -o $OUTPUT_FILE" \
    2>"$ELD_LOG" || EXIT_STATUS=$?

  if [[ $EXIT_STATUS -ne 0 ]]; then
    echo "    errors:" >&2
    grep -i "fatal\|error:" "$ELD_LOG" | head -5 >&2 || true
  fi

  WALL_SEC=$(python3 -c "
import json, sys
d = json.load(open('$HYPERFINE_JSON'))
print(round(d['results'][0]['mean'], 2))
" 2>/dev/null || echo "0")

  # Use /usr/bin/time only for peak memory (hyperfine does not measure RSS).
  /usr/bin/time -v -o "$TIME_LOG" "$ELD_BINARY" "@$FIXED_RESPONSE" -o "$OUTPUT_FILE" \
    2>/dev/null || true
  PEAK_KB=$(grep "Maximum resident" "$TIME_LOG" | grep -oP '\d+' | tail -1 || echo "0")
  PEAK_MB=$(python3 -c "print(round($PEAK_KB / 1024, 1))")

  echo "    done: ${WALL_SEC}s, ${PEAK_MB} MB, exit=${EXIT_STATUS}" >&2

  [[ $first -eq 0 ]] && echo ","
  first=0
  printf '  {"workload":"%s","wall_seconds":%s,"peak_memory_mb":%s,"exit_status":%d}' \
    "$WORKLOAD" "$WALL_SEC" "$PEAK_MB" "$EXIT_STATUS"
done

echo ""
echo "]"
