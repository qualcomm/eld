#!/usr/bin/env python3
"""Summarize ELD lit test results across all arch/option configurations.

Usage:
  eld-lit-summary.py [--detail] [--detail-codes CODE[,CODE...]] <results.json> ...

Each JSON file is the --output of a lit run for one arch+option configuration.
The configuration name is derived from the parent directory name by stripping
the trailing '-config' suffix (e.g. 'Hexagon-default-config' -> 'hexagon-default').

With --detail, test names are listed beneath each configuration row for all
non-PASS result codes (FAIL, XFAIL, UNSUPPORTED, XPASS, TIMEOUT).
Use --detail-codes UNSUPPORTED,XFAIL to restrict which codes are expanded.

Environment variables (overridden by the corresponding CLI flags):
  ELD_SUMMARY_DETAIL=1            equivalent to --detail
  ELD_SUMMARY_DETAIL_CODES=CODE,  equivalent to --detail-codes CODE,...
"""

import argparse
import json
import os
import sys

CODES = ["PASS", "FAIL", "XFAIL", "UNSUPPORTED", "XPASS", "TIMEOUT"]
DEFAULT_DETAIL_CODES = ["FAIL", "XFAIL", "UNSUPPORTED", "XPASS", "TIMEOUT"]


def config_label(json_path):
    dirname = os.path.basename(os.path.dirname(os.path.abspath(json_path)))
    if dirname.endswith("-config"):
        dirname = dirname[: -len("-config")]
    return dirname.lower()


def load_results(json_path):
    """Return (counts_dict, tests_by_code_dict, elapsed_seconds) or (None, None, None) on error."""
    try:
        with open(json_path, encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return None, None, None

    elapsed = data.get("elapsed")

    counts = {c: 0 for c in CODES}
    tests_by_code = {c: [] for c in CODES}
    for test in data.get("tests", []):
        code = test.get("code", "").upper()
        if code in counts:
            counts[code] += 1
            tests_by_code[code].append(test.get("name", "<unknown>"))
    return counts, tests_by_code, elapsed


def _format_elapsed(seconds):
    if seconds is None:
        return "N/A"
    minutes, secs = divmod(seconds, 60)
    if minutes:
        return f"{int(minutes)}m{secs:.1f}s"
    return f"{secs:.1f}s"


def print_summary(rows, detail_codes):
    col_width = {c: max(len(c), 7) for c in CODES}
    label_col = max((len(r[0]) for r in rows), default=len("Configuration"))
    label_col = max(label_col, len("Configuration"))
    time_col = len("TIME")

    header = f"{'Configuration':<{label_col}}"
    for c in CODES:
        header += f"  {c:>{col_width[c]}}"
    header += f"  {'TOTAL':>7}  {'TIME':>{time_col}}"
    print(header)
    print("-" * len(header))

    for label, counts, tests_by_code, elapsed in rows:
        if counts is None:
            print(f"{label:<{label_col}}  (no results)")
            continue

        total = sum(counts[c] for c in CODES)
        line = f"{label:<{label_col}}"
        for c in CODES:
            line += f"  {counts[c]:>{col_width[c]}}"
        line += f"  {total:>7}  {_format_elapsed(elapsed):>{time_col}}"
        print(line)

        if detail_codes and tests_by_code is not None:
            for code in detail_codes:
                names = tests_by_code.get(code, [])
                if not names:
                    continue
                print(f"  [{code}]")
                for name in sorted(names):
                    print(f"    {name}")


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "json_files",
        nargs="*",
        metavar="results.json",
        help="JSON result files produced by lit --output",
    )
    parser.add_argument(
        "--detail",
        action="store_true",
        default=False,
        help=(
            "List individual test names beneath each configuration row. "
            "By default shows: " + ", ".join(DEFAULT_DETAIL_CODES) + ". "
            "Use --detail-codes to restrict which codes are expanded."
        ),
    )
    parser.add_argument(
        "--detail-codes",
        metavar="CODE[,CODE...]",
        help=(
            "Comma-separated list of result codes to expand under --detail "
            "(implies --detail). Valid codes: " + ", ".join(CODES) + ". "
            "Default when --detail is used alone: " + ",".join(DEFAULT_DETAIL_CODES)
        ),
    )
    args = parser.parse_args(argv)

    # Environment-variable fallbacks — CLI flags take precedence.
    if not args.detail and os.environ.get("ELD_SUMMARY_DETAIL", "").strip() == "1":
        args.detail = True
    if not args.detail_codes and os.environ.get("ELD_SUMMARY_DETAIL_CODES", "").strip():
        args.detail_codes = os.environ["ELD_SUMMARY_DETAIL_CODES"].strip()

    if args.detail_codes:
        codes = [c.strip().upper() for c in args.detail_codes.split(",") if c.strip()]
        unknown = [c for c in codes if c not in CODES]
        if unknown:
            parser.error(
                f"Unknown result code(s): {', '.join(unknown)}. "
                f"Valid codes: {', '.join(CODES)}"
            )
        args.detail_codes = codes
        args.detail = True
    elif args.detail:
        args.detail_codes = DEFAULT_DETAIL_CODES

    return args


def main(argv):
    args = parse_args(argv)

    if not args.json_files:
        print("No result files provided.")
        return

    rows = []
    for path in args.json_files:
        label = config_label(path)
        counts, tests_by_code, elapsed = load_results(path)
        rows.append((label, counts, tests_by_code, elapsed))

    print_summary(rows, args.detail_codes or [])


if __name__ == "__main__":
    main(sys.argv[1:])
