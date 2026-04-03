#!/usr/bin/env python3
#"===----------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
#"===----------------------------------------------------------------------===
"""
check_syntax.py - Query Vim syntax highlighting groups for a file.

Drives vim in batch mode to query synID() at specific (line, col) positions
and verify the expected syntax group names.

Usage:
  check_syntax.py --vim VIM_PATH --syntax SYNTAX_FILE --input INPUT_FILE
                  --check LINE:COL:EXPECTED [--check ...]

  --vim       Path to the vim binary (auto-detected from PATH if omitted).
              Must be vim (not nvim); nvim is not supported.
  --syntax    Path to the .vim syntax file to source (e.g. eld.vim).
  --input     Path to the file to open in vim for syntax checking.
  --check     One or more LINE:COL:EXPECTED assertions.
              LINE and COL are 1-based.  EXPECTED is the syntax group name
              (e.g. eldComment).  Use an empty string to assert no group.
  --dump      Instead of checking, dump all non-empty groups for every
              character in the file (useful for writing new tests).

Exit codes:
  0  All assertions passed (or --dump completed successfully).
  1  One or more assertions failed, or an error occurred.

How it works
------------
vim is invoked in batch mode (-T dumb, no -n) with a generated vimscript
that:
  1. Opens the input file.
  2. Sets filetype=eld so that syntax autocommands fire.
  3. Sources the syntax file explicitly (so the groups are defined even
     without a proper runtimepath install).
  4. Calls synID(line, col, 1) for each requested position and writes the
     results to a temp file.
  5. Quits.

The key requirements for synID() to work in vim batch mode:
  - Do NOT use -n (no-swap-file) flag; it disables syntax.
  - Set filetype BEFORE sourcing the syntax file.
  - Source the syntax file explicitly (do not rely on autocommands).
  - Use -T dumb (not xterm) to avoid terminal-init overhead.
  - Redirect stdin from /dev/null so vim doesn't wait for input.
"""

import argparse
import os
import subprocess
import sys
import tempfile


# ---------------------------------------------------------------------------
# Editor discovery
# ---------------------------------------------------------------------------

def _find_vim():
    """Return the path to vim, or None if not found."""
    import shutil
    # Prefer an explicit 'vim' over 'vi' or other aliases.
    for name in ("vim",):
        path = shutil.which(name)
        if path:
            return path
    return None


# ---------------------------------------------------------------------------
# Vimscript generation
# ---------------------------------------------------------------------------

def _make_vimscript(syntax_file, input_file, positions, result_path):
    """
    Return a vimscript string that opens *input_file*, sources *syntax_file*,
    queries synID at each (line, col) in *positions*, writes results to
    *result_path*, and quits.

    The script is designed to be passed as -u <script> to vim.
    """
    sf = syntax_file.replace("\\", "/")
    inf = input_file.replace("\\", "/")
    rp = result_path.replace("\\", "/")

    lines = [
        "set nocompatible",
        "syntax on",
        # Open the file to check.
        "edit " + inf,
        # Set filetype BEFORE sourcing so that b:current_syntax guard fires
        # correctly and syntax autocommands have already run.
        "set filetype=eld",
        # Source the syntax file explicitly.
        "source " + sf,
        # Build the results list.
        "let g:eld_results = []",
    ]

    for lnum, col in positions:
        lines.append(
            "call add(g:eld_results, "
            "'{lnum}:{col}:' . synIDattr(synID({lnum},{col},1),'name'))".format(
                lnum=lnum, col=col
            )
        )

    lines += [
        "call writefile(g:eld_results, '" + rp + "')",
        "qa!",
    ]

    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Running vim
# ---------------------------------------------------------------------------

def _run_vim(vim_path, vimscript_path, timeout=30):
    """
    Run vim with *vimscript_path* as the -u script.

    Returns (returncode, stderr_text).
    """
    cmd = [
        vim_path,
        "-X",          # Don't connect to X server.
        "-T", "dumb",  # Dumb terminal; avoids terminal-init sequences.
        "-u", vimscript_path,
    ]
    proc = subprocess.run(
        cmd,
        stdin=open(os.devnull),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
    )
    return proc.returncode, proc.stderr.decode(errors="replace")


# ---------------------------------------------------------------------------
# High-level query
# ---------------------------------------------------------------------------

def query_positions(vim_path, syntax_file, input_file, positions):
    """
    Query synID for each (line, col) in *positions*.

    Returns a dict mapping (line, col) -> syngroup_name (may be empty string).
    Raises RuntimeError on failure.
    """
    # Write results to a temp file.
    fd, result_path = tempfile.mkstemp(suffix=".txt", prefix="eld_syn_")
    os.close(fd)

    # Write the vimscript to a temp file.
    fd2, script_path = tempfile.mkstemp(suffix=".vim", prefix="eld_syn_")
    try:
        script = _make_vimscript(syntax_file, input_file, positions, result_path)
        with os.fdopen(fd2, "w") as f:
            f.write(script)

        rc, stderr = _run_vim(vim_path, script_path)
        # vim exits 0 on success; exit 1 is also acceptable (e.g. swap-file
        # warnings in batch mode).
        if rc not in (0, 1):
            raise RuntimeError(
                "vim exited with code {}:\n{}".format(rc, stderr)
            )

        with open(result_path) as f:
            raw = f.read().strip()

        result = {}
        for line in raw.splitlines():
            parts = line.split(":", 2)
            if len(parts) == 3:
                result[(int(parts[0]), int(parts[1]))] = parts[2]
            elif len(parts) == 2:
                # Empty group name.
                result[(int(parts[0]), int(parts[1]))] = ""
        return result

    finally:
        os.unlink(script_path)
        if os.path.exists(result_path):
            os.unlink(result_path)


# ---------------------------------------------------------------------------
# Dump mode
# ---------------------------------------------------------------------------

def dump_all(vim_path, syntax_file, input_file):
    """Print LINE:COL:GROUP for every non-empty group in the file."""
    with open(input_file) as f:
        file_lines = f.readlines()

    positions = []
    for lnum, line in enumerate(file_lines, 1):
        for col in range(1, len(line.rstrip("\n")) + 1):
            positions.append((lnum, col))

    result = query_positions(vim_path, syntax_file, input_file, positions)

    for lnum, line in enumerate(file_lines, 1):
        for col in range(1, len(line.rstrip("\n")) + 1):
            group = result.get((lnum, col), "")
            if group:
                char = line[col - 1]
                print("{}:{}:{}:{}".format(lnum, col, group, char))


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Check vim syntax groups at specific positions in a file."
    )
    parser.add_argument(
        "--vim",
        help="Path to the vim binary (auto-detected if omitted).",
    )
    parser.add_argument(
        "--syntax",
        required=True,
        help="Path to the .vim syntax file to source.",
    )
    parser.add_argument(
        "--input",
        required=True,
        help="Path to the file to open in vim.",
    )
    parser.add_argument(
        "--check",
        action="append",
        metavar="LINE:COL:EXPECTED",
        help=(
            "Assert that position LINE:COL has syntax group EXPECTED. "
            "Repeat for multiple checks. Use an empty EXPECTED to assert "
            "no group is active."
        ),
    )
    parser.add_argument(
        "--dump",
        action="store_true",
        help="Dump all non-empty syntax groups for every character.",
    )
    args = parser.parse_args()

    # Locate vim.
    if args.vim:
        vim_path = args.vim
    else:
        vim_path = _find_vim()
        if not vim_path:
            print("ERROR: vim not found in PATH", file=sys.stderr)
            sys.exit(1)

    syntax_file = os.path.abspath(args.syntax)
    input_file = os.path.abspath(args.input)

    if args.dump:
        try:
            dump_all(vim_path, syntax_file, input_file)
        except Exception as exc:
            print("ERROR: {}".format(exc), file=sys.stderr)
            sys.exit(1)
        return

    if not args.check:
        parser.error("Provide --check LINE:COL:EXPECTED or --dump")

    # Parse check specs.
    checks = []
    for spec in args.check:
        parts = spec.split(":", 2)
        if len(parts) != 3:
            print("ERROR: bad --check spec: {!r}".format(spec), file=sys.stderr)
            sys.exit(1)
        checks.append((int(parts[0]), int(parts[1]), parts[2]))

    positions = [(lnum, col) for lnum, col, _ in checks]

    try:
        result = query_positions(vim_path, syntax_file, input_file, positions)
    except Exception as exc:
        print("ERROR: {}".format(exc), file=sys.stderr)
        sys.exit(1)

    failed = False
    for lnum, col, expected in checks:
        actual = result.get((lnum, col), "")
        status = "PASS" if actual == expected else "FAIL"
        print("{} {}:{} expected={!r} actual={!r}".format(
            status, lnum, col, expected, actual))
        if actual != expected:
            failed = True

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
