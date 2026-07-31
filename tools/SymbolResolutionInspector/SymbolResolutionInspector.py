#!/usr/bin/env python3
"""Inspect an ELD symbol resolution report (--emit-symbol-resolution-report).

Reads the JSON report and lets you filter symbols by name or by the
reconstructed per-candidate info string.
"""
import argparse
import json
import re
import sys

KNOWN_VERSION = 1


def candidate_info_string(c):
    """Reconstruct a candidate's info string, matching the linker's rendering."""
    loc = c["InputFile"]
    if c.get("Plugin"):
        loc += "[" + c["Plugin"] + "]"
    if c.get("Section"):
        loc += ":" + c["Section"]
    attrs = ["Size=%d" % c.get("Size", 0)]
    if c.get("Bitcode"):
        attrs.append("bitcode")
    attrs += [c["SectionIndexKind"], c["Binding"], c["Type"], c["Visibility"]]
    return "%s(%s) [%s]" % (c["Name"], loc, ", ".join(attrs))


def symbol_matches(sym, name_re, filter_re):
    if name_re and not name_re.search(sym["Name"]):
        return False
    if filter_re:
        cands = sym.get("Candidates", [])
        if not any(filter_re.search(candidate_info_string(c)) for c in cands):
            return False
    return True


def print_symbol(sym):
    print(sym["Name"])
    if sym.get("Selected"):
        print("  Selected: %s" % sym["Selected"])
    for c in sym.get("Candidates", []):
        mark = " [Selected]" if c.get("IsSelected") else ""
        print("    %s%s" % (candidate_info_string(c), mark))
        lto = c.get("LTOObjectSymbol")
        if lto:
            print("      LTO: %s" % candidate_info_string(lto))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("report", help="JSON report from --emit-symbol-resolution-report")
    p.add_argument("--symbol", help="regex matched against symbol Name")
    p.add_argument("--filter", help="regex matched against per-candidate info string")
    p.add_argument("--json", action="store_true", help="emit matching symbols as JSON")
    args = p.parse_args()

    with open(args.report) as f:
        report = json.load(f)

    version = report.get("SymbolResolutionReportVersion")
    if version is not None and version > KNOWN_VERSION:
        sys.stderr.write(
            "warning: report version %s is newer than this tool (%d)\n"
            % (version, KNOWN_VERSION))

    name_re = re.compile(args.symbol) if args.symbol else None
    filter_re = re.compile(args.filter) if args.filter else None

    matched = [s for s in report.get("Symbols", [])
               if symbol_matches(s, name_re, filter_re)]

    if args.json:
        json.dump({"SymbolResolutionReportVersion": version, "Symbols": matched},
                  sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        for s in matched:
            print_symbol(s)
        print("%d of %d symbols matched" % (len(matched),
                                            len(report.get("Symbols", []))))


if __name__ == "__main__":
    main()
