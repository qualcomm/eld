#!/usr/bin/env python3
# "===----------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
# "===----------------------------------------------------------------------===
"""Inspect an ELD symbol resolution report (--emit-symbol-resolution-report).

Reads the JSON report and lets you filter symbols by name or by the
per-candidate info string.

--symbol selects whole symbols by name. --filter selects individual
candidates by their info string: a symbol is shown only if at least one of
its candidates matches, and only the matching candidates are printed (the
non-matching candidates of that symbol are dropped from both the text and
JSON output).
"""

import argparse
import json
import re
import sys

KNOWN_VERSION = 1


def candidate_info_string(c):
    """Create a candidate's info string, matching the linker's rendering.

    For a candidate that has every property set, the result looks like:

        foo(a.bc[LTOPlugin]:.text) [Size=16, bitcode, Def, SB_Global, Function, Default]

    where the parenthesized part is InputFile[Plugin]:Section and the
    bracketed part is Size, an optional "bitcode" marker, SectionIndexKind,
    Binding, Type, and Visibility.
    """
    loc = c["InputFile"]
    if c.get("Plugin"):
        loc += "[" + c["Plugin"] + "]"
    if c.get("Section"):
        loc += ":" + c["Section"]
    attrs = ["Size={}".format(c.get("Size", 0))]
    if c.get("Bitcode"):
        attrs.append("bitcode")
    attrs += [c["SectionIndexKind"], c["Binding"], c["Type"], c["Visibility"]]
    return "{name}({loc}) [{attrs}]".format(
        name=c["Name"], loc=loc, attrs=", ".join(attrs)
    )


def select_symbol(sym, name_re, filter_re):
    """Return the symbol to display, or None if it does not match.

    --symbol matches the whole symbol by Name. --filter is candidate-level:
    the returned symbol keeps only the candidates whose info string matches,
    and the symbol is dropped entirely if none match.
    """
    if name_re and not name_re.search(sym["Name"]):
        return None
    if not filter_re:
        return sym
    cands = [
        c
        for c in sym.get("Candidates", [])
        if filter_re.search(candidate_info_string(c))
    ]
    if not cands:
        return None
    filtered = dict(sym)
    filtered["Candidates"] = cands
    return filtered


def print_symbol(sym):
    print(sym["Name"])
    if sym.get("Selected"):
        print("\tSelected: {}".format(sym["Selected"]))
    for c in sym.get("Candidates", []):
        mark = " [Selected]" if c.get("IsSelected") else ""
        print("\t{}{}".format(candidate_info_string(c), mark))
        lto = c.get("LTOObjectSymbol")
        if lto:
            print("\t\tLTO: {}".format(candidate_info_string(lto)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("report", help="JSON report from --emit-symbol-resolution-report")
    p.add_argument("--symbol", help="regex selecting whole symbols by Name")
    p.add_argument(
        "--filter",
        help="regex matched against each candidate's info string; "
        "shows only the matching candidates.",
    )
    p.add_argument("--json", action="store_true", help="emit matching symbols as JSON")
    args = p.parse_args()

    with open(args.report) as f:
        report = json.load(f)

    version = report.get("SymbolResolutionReportVersion")
    if version is not None and version > KNOWN_VERSION:
        sys.stderr.write(
            "warning: report version {} is newer than this tool ({})\n".format(
                version, KNOWN_VERSION
            )
        )

    name_re = re.compile(args.symbol) if args.symbol else None
    filter_re = re.compile(args.filter) if args.filter else None

    matched = [
        m
        for m in (
            select_symbol(s, name_re, filter_re) for s in report.get("Symbols", [])
        )
        if m is not None
    ]

    if args.json:
        json.dump(
            {"SymbolResolutionReportVersion": version, "Symbols": matched},
            sys.stdout,
            indent=2,
        )
        sys.stdout.write("\n")
    else:
        for s in matched:
            print_symbol(s)
        print(
            "{} of {} symbols matched".format(
                len(matched), len(report.get("Symbols", []))
            )
        )


if __name__ == "__main__":
    main()
