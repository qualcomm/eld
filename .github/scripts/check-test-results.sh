#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBC_TEST_DIR="${1}"
EXPECTED_FAILURES_FILE="$SCRIPT_DIR/expected-failures.txt"
REPORT_FILE="$LIBC_TEST_DIR/src/REPORT"

# Extract actual failures
actual_failures=$(mktemp)
grep "^FAIL" "$REPORT_FILE" > "$actual_failures" || touch "$actual_failures"

# Find unexpected failures (in actual but not in expected)
unexpected_failures=$(mktemp)
comm -23 <(sort "$actual_failures") <(sort "$EXPECTED_FAILURES_FILE") > "$unexpected_failures"

# Check result
unexpected_count=$(wc -l < "$unexpected_failures")
if [ $unexpected_count -gt 0 ]; then
    echo "ERROR: $unexpected_count unexpected failure(s) found:"
    cat "$unexpected_failures"
    rm -f "$actual_failures" "$unexpected_failures"
    exit 1
else
    echo "SUCCESS: No unexpected failures detected"
    rm -f "$actual_failures" "$unexpected_failures"
    exit 0
fi

