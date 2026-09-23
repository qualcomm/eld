#!/usr/bin/env python3
# "===----------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
# "===----------------------------------------------------------------------===
"""Inspect an ELD symbol resolution report (--emit-symbol-resolution-report).

Reads the JSON report and lets you filter symbols by name or by the symbol info
string.

--symbol selects symbols by name. --filter selects symbols by their info string.
"""

import argparse
import json
import re
import sys

KNOWN_VERSION = 1


def symbol_info_string(sym):
    """Create a symbol's info string.

    For a symbol that has every property set, the result looks like:

        foo(a.bc[LTOPlugin]:.text) [Size=16, bitcode, Def, SB_Global, Function, Default]

    where the parenthesized part is InputFile[Plugin]:Section and the
    bracketed part is Size, an optional "bitcode" marker, SectionIndexKind,
    Binding, Type, and Visibility.
    """
    loc = sym["InputFile"]
    if sym.get("Plugin"):
        loc += "[" + sym["Plugin"] + "]"
    if sym.get("Section"):
        loc += ":" + sym["Section"]
    attrs = ["Size={}".format(sym.get("Size", 0))]
    if sym.get("Bitcode"):
        attrs.append("bitcode")
    attrs += [sym["SectionIndexKind"], sym["Binding"], sym["Type"], sym["Visibility"]]
    return "{name}({loc}) [{attrs}]".format(
        name=sym["Name"], loc=loc, attrs=", ".join(attrs)
    )


def should_select_symbol(sym, name_re, filter_re):
    """Return true if the symbol should be displayed.

    --symbol matches by Name. --filter matches the rendered symbol info string.
    """
    if name_re and not name_re.search(sym["Name"]):
        return False
    if filter_re and not filter_re.search(symbol_info_string(sym)):
        return False
    return True


def selected_label(sym):
    return "{}({})".format(sym["InputFile"], sym["Name"])


def print_symbols(symbols):
    groups = []
    by_name = {}
    for sym in symbols:
        group = by_name.get(sym["Name"])
        if group is None:
            group = []
            by_name[sym["Name"]] = group
            groups.append((sym["Name"], group))
        group.append(sym)

    for name, group in groups:
        print(name)
        selected = next((s for s in group if s.get("IsSelected")), None)
        if selected:
            print("\tSelected: {}".format(selected_label(selected)))
        for sym in group:
            mark = " [Selected]" if sym.get("IsSelected") else ""
            print("\t{}{}".format(symbol_info_string(sym), mark))
            lto = sym.get("LTOObjectSymbol")
            if lto:
                print("\t\tLTO: {}".format(symbol_info_string(lto)))


def create_arg_parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("report", help="JSON report from --emit-symbol-resolution-report")
    p.add_argument("--symbol", help="regex selecting whole symbols by Name")
    p.add_argument(
        "--filter",
        help="regex matched against each symbol's info string; "
        "shows only the matching symbols.",
    )
    p.add_argument("--json", action="store_true", help="emit matching symbols as JSON")
    return p


def main():
    p = create_arg_parser()
    args = p.parse_args()

    with open(args.report) as f:
        report = json.load(f)

    version = report.get("SymbolResolutionReportVersion")
    if version is None:
        p.error("missing SymbolResolutionReportVersion")
    if version > KNOWN_VERSION:
        sys.stderr.write(
            "warning: report version {} is newer than what this tool supports ({})\n".format(
                version, KNOWN_VERSION
            )
        )

    name_re = re.compile(args.symbol) if args.symbol else None
    filter_re = re.compile(args.filter) if args.filter else None

    matched = [
        s
        for s in report.get("Symbols", [])
        if should_select_symbol(s, name_re, filter_re)
    ]

    if args.json:
        json.dump(
            {"SymbolResolutionReportVersion": version, "Symbols": matched},
            sys.stdout,
            indent=2,
        )
        sys.stdout.write("\n")
    else:
        print_symbols(matched)
        print(
            "{} of {} symbols matched".format(
                len(matched), len(report.get("Symbols", []))
            )
        )


if __name__ == "__main__":
    main()
