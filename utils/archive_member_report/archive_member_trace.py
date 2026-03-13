#!/usr/bin/env python3
import argparse
import fnmatch
import json
import sys
from typing import Dict, List, Set, Tuple



def get_referrer_path(record: dict) -> str:
    """Return a printable referrer path from split/legacy JSON fields."""
    archive = record.get("ReferrerArchive")
    member = record.get("ReferrerMember")
    if archive and member:
        return f"{archive}({member})"
    obj = record.get("ReferrerObjectFile")
    if obj:
        return obj
    if record.get("WholeArchive"):
        return "-whole-archive"
    ref = record.get("Referrer")
    if ref:
        return ref
    return "<unknown referrer>"


def is_referrer_archive_member(record: dict) -> bool:
    """Return whether this edge was requested by another archive member."""
    return bool(record.get("ReferrerIsArchiveMember", False))


def load_records(path: str) -> List[dict]:
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    records = data.get("ArchiveMembers", [])
    if not isinstance(records, list):
        raise ValueError("ArchiveMembers must be a list")
    return records


def build_member_index(records: List[dict]) -> Dict[str, List[dict]]:
    """Index all records by MemberPath to enable reverse chain traversal."""
    index: Dict[str, List[dict]] = {}
    for r in records:
        member = r.get("MemberPath")
        if not member:
            continue
        index.setdefault(member, []).append(r)
    return index


def symbol_matches_from_list(symbols: object, pattern: str) -> bool:
    if not isinstance(symbols, list):
        return False
    for sym in symbols:
        if fnmatch.fnmatch(sym or "", pattern):
            return True
    return False


def format_chain(chain: List[dict]) -> str:
    parts: List[str] = []
    for rec in chain:
        referrer = get_referrer_path(rec)
        symbol = rec.get("Symbol", "<unknown symbol>")
        member_path = rec.get("MemberPath", "<unknown member>")
        parts.append(f"{referrer} --{symbol}--> {member_path}")
    return " | ".join(parts)


def build_member_referenced_symbol_index(records: List[dict]) -> Dict[str, Set[str]]:
    """Build compatibility index for reports without MemberReferencedSymbols."""
    index: Dict[str, Set[str]] = {}
    for record in records:
        referrer = get_referrer_path(record)
        symbol = record.get("Symbol")
        if not referrer or not symbol:
            continue
        index.setdefault(referrer, set()).add(symbol)
    return index


def get_member_referenced_symbols(
    record: dict, fallback_index: Dict[str, Set[str]]
) -> Set[str]:
    symbols = record.get("MemberReferencedSymbols")
    if isinstance(symbols, list):
        return {s for s in symbols if s}
    member_path = record.get("MemberPath", "")
    return fallback_index.get(member_path, set())


def print_grouped_symbols(
    groups: Dict[str, Set[str]],
    entity_label: str,
    descriptor: str,
    pattern: str,
) -> int:
    if not groups:
        print(f"no {descriptor} found for {entity_label.lower()} pattern: {pattern}")
        return 2

    print(f"{descriptor} for {entity_label.lower()} pattern: {pattern}")
    print(f"unique {entity_label.lower()}s: {len(groups)}")
    for entity in sorted(groups):
        symbols = groups[entity]
        print()
        print(f"{entity_label}: {entity}")
        print(f"symbol-count: {len(symbols)}")
        for symbol in sorted(symbols):
            print(f"  symbol: {symbol}")
    return 0


def build_paths(
    record: dict,
    member_index: Dict[str, List[dict]],
    visited: Set[Tuple[str, str]],
    current: List[dict],
    paths: List[List[dict]],
) -> None:
    # DFS from a pulled-in member to its upstream requester(s).
    # `visited` avoids infinite recursion in cyclic start-group scenarios.
    member = record.get("MemberPath", "<unknown member>")
    referrer = get_referrer_path(record)
    key = (member, referrer)
    if key in visited:
        paths.append(current + [record])
        return
    visited.add(key)
    current.append(record)

    if is_referrer_archive_member(record):
        upstream = member_index.get(referrer, [])
        if upstream:
            for up in upstream:
                build_paths(up, member_index, visited, current, paths)
        else:
            paths.append(current.copy())
    else:
        paths.append(current.copy())

    current.pop()


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Trace why an archive member was pulled in, using "
            "ELD's archive member JSON report."
        )
    )
    parser.add_argument("json", help="Path to archive member JSON report")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--pattern",
        help=(
            "Wildcard pattern to match archive members "
            "(matches MemberPath or Member)"
        ),
    )
    group.add_argument(
        "--symbol",
        help="Symbol name to trace (matches the report's Symbol field)",
    )
    group.add_argument(
        "--defined-symbols-archive",
        help="Print defined symbols grouped by archive (archive glob pattern)",
    )
    group.add_argument(
        "--referenced-symbols-archive",
        help=(
            "Print referenced symbols grouped by archive "
            "(archive glob pattern)"
        ),
    )
    group.add_argument(
        "--defined-symbols-member",
        help=(
            "Print defined symbols grouped by archive member "
            "(matches MemberPath or Member)"
        ),
    )
    group.add_argument(
        "--referenced-symbols-member",
        help=(
            "Print referenced symbols grouped by archive member "
            "(matches MemberPath or Member)"
        ),
    )
    parser.add_argument(
        "--symbols-only",
        action="store_true",
        help="Print only the symbol chain for each trace",
    )
    parser.add_argument(
        "--why-needed",
        action="store_true",
        help=(
            "For --symbol queries, match only the report Symbol field "
            "(skip alias/member symbol matches) and show why that symbol was needed"
        ),
    )
    args = parser.parse_args()

    if args.why_needed and not args.symbol:
        parser.error("--why-needed requires --symbol")

    try:
        records = load_records(args.json)
    except Exception as exc:
        print(f"error: failed to read {args.json}: {exc}", file=sys.stderr)
        return 1

    member_index = build_member_index(records)
    member_referenced_symbol_index = build_member_referenced_symbol_index(records)

    if args.defined_symbols_archive:
        groups: Dict[str, Set[str]] = {}
        for r in records:
            archive = r.get("Archive", "")
            if not archive or not fnmatch.fnmatch(archive, args.defined_symbols_archive):
                continue
            member_symbols = r.get("MemberSymbols")
            if not isinstance(member_symbols, list):
                continue
            groups.setdefault(archive, set()).update(s for s in member_symbols if s)
        return print_grouped_symbols(
            groups,
            "Archive",
            "Defined symbols",
            args.defined_symbols_archive,
        )

    if args.referenced_symbols_archive:
        groups = {}
        for r in records:
            archive = r.get("Archive", "")
            if not archive or not fnmatch.fnmatch(archive, args.referenced_symbols_archive):
                continue
            refs = get_member_referenced_symbols(r, member_referenced_symbol_index)
            if not refs:
                continue
            groups.setdefault(archive, set()).update(refs)
        return print_grouped_symbols(
            groups,
            "Archive",
            "Referenced symbols",
            args.referenced_symbols_archive,
        )

    if args.defined_symbols_member:
        groups = {}
        for r in records:
            member_path = r.get("MemberPath", "")
            member_name = r.get("Member", "")
            if not (
                fnmatch.fnmatch(member_path, args.defined_symbols_member)
                or fnmatch.fnmatch(member_name, args.defined_symbols_member)
            ):
                continue
            member_symbols = r.get("MemberSymbols")
            if not isinstance(member_symbols, list):
                continue
            groups.setdefault(member_path, set()).update(s for s in member_symbols if s)
        return print_grouped_symbols(
            groups,
            "Member",
            "Defined symbols",
            args.defined_symbols_member,
        )

    if args.referenced_symbols_member:
        groups = {}
        for r in records:
            member_path = r.get("MemberPath", "")
            member_name = r.get("Member", "")
            if not (
                fnmatch.fnmatch(member_path, args.referenced_symbols_member)
                or fnmatch.fnmatch(member_name, args.referenced_symbols_member)
            ):
                continue
            refs = get_member_referenced_symbols(r, member_referenced_symbol_index)
            if not refs:
                continue
            groups.setdefault(member_path, set()).update(refs)
        return print_grouped_symbols(
            groups,
            "Member",
            "Referenced symbols",
            args.referenced_symbols_member,
        )

    # Symbol/pattern selection stage:
    # - --pattern matches member names/paths
    # - --symbol matches pull-in symbol, with compatibility fallbacks
    has_member_referenced_symbols = any(
        "MemberReferencedSymbols" in r for r in records
    )
    warned_legacy_fallback = False
    matches: List[dict] = []
    if args.pattern:
        for r in records:
            member_path = r.get("MemberPath", "")
            member_name = r.get("Member", "")
            if fnmatch.fnmatch(member_path, args.pattern) or fnmatch.fnmatch(
                member_name, args.pattern
            ):
                matches.append(r)
    else:
        for r in records:
            if args.why_needed and r.get("WholeArchive"):
                continue
            sym = r.get("Symbol") or ""
            if fnmatch.fnmatch(sym, args.symbol):
                matches.append(r)
                continue
            if not args.why_needed:
                if symbol_matches_from_list(r.get("MemberSymbols"), args.symbol):
                    matches.append(r)
                    continue
            if args.why_needed and symbol_matches_from_list(
                r.get("MemberReferencedSymbols"), args.symbol
            ):
                matches.append(r)
                continue
            if args.why_needed and not has_member_referenced_symbols:
                if symbol_matches_from_list(r.get("MemberSymbols"), args.symbol):
                    matches.append(r)
                    warned_legacy_fallback = True
                    continue
    if not matches:
        if args.pattern:
            print(f"no records found for pattern: {args.pattern}")
        else:
            print(f"no records found for symbol: {args.symbol}")
        return 2

    if warned_legacy_fallback:
        print(
            "warning: report lacks MemberReferencedSymbols; "
            "falling back to MemberSymbols matching"
        )

    if args.pattern:
        print(f"Trace for pattern: {args.pattern}")
    elif args.why_needed:
        print(f"Why symbol needed: {args.symbol}")
    else:
        print(f"Trace for symbol: {args.symbol}")

    # Group by target member so each member's traces are printed together.
    grouped: Dict[str, List[dict]] = {}
    for r in matches:
        member = r.get("MemberPath", "<unknown member>")
        grouped.setdefault(member, []).append(r)

    print(f"unique members: {len(grouped)}")
    for member, records_for_member in grouped.items():
        print()
        print(f"member: {member}")
        trace_idx = 0
        for r in records_for_member:
            visited = set()
            paths: List[List[dict]] = []
            build_paths(r, member_index, visited, [], paths)
            if not paths:
                print("  [no trace paths found]")
                continue
            for path in paths:
                trace_idx += 1
                print(f"  Trace {trace_idx}")
                chain = list(reversed(path))
                if args.why_needed and not args.symbols_only and chain:
                    leaf = chain[-1]
                    requested_by = get_referrer_path(leaf)
                    print(f"    why-symbol: {leaf.get('Symbol', '<unknown symbol>')}")
                    print(f"    requested-by: {requested_by}")
                    print(
                        f"    because: {requested_by} references "
                        f"{leaf.get('Symbol', '<unknown symbol>')}"
                    )
                    print(f"    chain: {format_chain(chain)}")
                    refs = leaf.get("MemberReferencedSymbols")
                    if symbol_matches_from_list(refs, args.symbol):
                        print(f"    member-references: {args.symbol}")
                for depth, rec in enumerate(chain, 1):
                    referrer = get_referrer_path(rec)
                    symbol = rec.get("Symbol", "<unknown symbol>")
                    member_path = rec.get("MemberPath", "<unknown member>")
                    if args.symbols_only:
                        print(f"    symbol: {symbol}")
                        continue
                    print(f"    #{depth}")
                    print(f"      referrer: {referrer}")
                    print(f"      symbol: {symbol}")
                    print(f"      member: {member_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
