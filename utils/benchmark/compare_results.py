#!/usr/bin/env python3
"""
Compare ELD benchmark results between PR branch and baseline (main tip).
Outputs a markdown comment suitable for posting on a GitHub PR.

Usage:
    compare_results.py --pr results-pr.json --baseline results-main.json [--threshold 5]
"""

import argparse
import json
import sys


def load(path):
    with open(path) as f:
        data = json.load(f)
    return {item["workload"]: item for item in data}


def pct_change(new, old):
    if old == 0:
        return 0.0
    return (new - old) / old * 100


def fmt_pct(val):
    sign = "+" if val > 0 else ""
    return f"{sign}{val:.1f}%"


def regression_icon(wall_pct, mem_pct, threshold):
    if wall_pct > threshold or mem_pct > threshold:
        return "⚠️"
    if wall_pct < -threshold or mem_pct < -threshold:
        return "✅"
    return "➖"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pr", required=True)
    parser.add_argument("--baseline", required=False, default=None)
    parser.add_argument("--threshold", type=float, default=5.0,
                        help="Percentage change threshold for regression warning")
    args = parser.parse_args()

    pr = load(args.pr)
    baseline = load(args.baseline) if args.baseline else None
    threshold = args.threshold

    lines = []
    lines.append("## ELD Linker Benchmark Results")
    lines.append("")

    if baseline is None:
        # First run — no baseline exists yet, just report absolute numbers
        lines.append("> ℹ️ **No baseline available yet.** This run will be used as the baseline for future comparisons.")
        lines.append("")
        lines.append("| Workload | Wall time | Peak memory | Status |")
        lines.append("|----------|-----------|-------------|--------|")
        for wl in sorted(pr):
            p = pr[wl]
            status = "✅" if p["exit_status"] == 0 else "❌"
            lines.append(f"| {wl} | {p['wall_seconds']:.2f}s | {p['peak_memory_mb']:.0f} MB | {status} |")
        print("\n".join(lines))
        sys.exit(0)

    all_workloads = sorted(set(pr) | set(baseline))

    rows = []
    has_regression = False
    has_improvement = False

    for wl in all_workloads:
        if wl not in pr:
            rows.append((wl, "—", "—", "—", "—", "⚪ missing in PR"))
            continue
        if wl not in baseline:
            rows.append((wl, f"{pr[wl]['wall_seconds']:.2f}s",
                         f"{pr[wl]['peak_memory_mb']:.0f} MB", "—", "—", "⚪ new workload"))
            continue

        p = pr[wl]
        b = baseline[wl]

        wall_pct = pct_change(p["wall_seconds"], b["wall_seconds"])
        mem_pct = pct_change(p["peak_memory_mb"], b["peak_memory_mb"])
        icon = regression_icon(wall_pct, mem_pct, threshold)

        if icon == "⚠️":
            has_regression = True
        if icon == "✅":
            has_improvement = True

        rows.append((
            wl,
            f"{p['wall_seconds']:.2f}s",
            f"{p['peak_memory_mb']:.0f} MB",
            fmt_pct(wall_pct),
            fmt_pct(mem_pct),
            icon,
        ))

    if has_regression:
        lines.append(f"> ⚠️ **Performance regression detected** (>{threshold:.0f}% slower or higher memory than main tip)")
    elif has_improvement:
        lines.append(f"> ✅ **Performance improvement detected** vs main tip")
    else:
        lines.append(f"> ➖ No significant performance change vs main tip (threshold: {threshold:.0f}%)")
    lines.append("")

    lines.append("| Workload | PR wall time | PR memory | Δ wall | Δ memory | |")
    lines.append("|----------|-------------|-----------|--------|----------|---|")
    for row in rows:
        lines.append("| {} | {} | {} | {} | {} | {} |".format(*row))

    lines.append("")
    lines.append("<details>")
    lines.append("<summary>Baseline (main tip)</summary>")
    lines.append("")
    lines.append("| Workload | Wall time | Memory |")
    lines.append("|----------|-----------|--------|")
    for wl in all_workloads:
        if wl in baseline:
            b = baseline[wl]
            lines.append(f"| {wl} | {b['wall_seconds']:.2f}s | {b['peak_memory_mb']:.0f} MB |")
    lines.append("")
    lines.append("</details>")

    print("\n".join(lines))

    sys.exit(1 if has_regression else 0)


if __name__ == "__main__":
    main()
