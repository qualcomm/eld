#!/usr/bin/env python3
#"===------------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
#"===------------------------------------------------------------------------===

"""Framework manager for ELD relocation testing.

Owns CLI parsing, the arch registry, and dispatch to the two testing modes:
static_analysis.py (link-success/failure + static ELF layout) and
run_analysis.py (qemu-based runtime checks).
"""

import argparse
import sys

import run_analysis
import static_analysis

import arch.arm.arch

ARCH = {
    "arm": arch.arm.arch.ARCH,
}

MODES = ["static", "run", "both"]

def create_argparser():
    argparser = argparse.ArgumentParser()
    argparser.add_argument("--arch",
                           dest="arch",
                           help=f"Arch, one of [{','.join(ARCH.keys())}]")
    argparser.add_argument("--ld",
                           dest="link_cmd",
                           help="Linker under test command line")
    argparser.add_argument("--output-dir",
                           dest="output_dir",
                           help="Output directory")
    argparser.add_argument("--mode",
                           dest="mode",
                           choices=MODES,
                           default="both",
                           help="Which testing mode(s) to run (default: both)")
    return argparser

def main():
    args = create_argparser().parse_args()
    output_dir = args.output_dir if args.output_dir else "."
    link_cmd = args.link_cmd.split(
    ) if args.link_cmd else static_analysis.DEFAULT_LINK  # TODO: spaces and quotes are probably not handled well.

    if not args.arch in ARCH:
        print(f"Don't know anything about architecture {args.arch}",
              file=sys.stderr)
        sys.exit(1)

    arch_name = args.arch
    arch_info = ARCH[arch_name]

    if args.mode in ("static", "both"):
        static_analysis.run(arch_name, arch_info, link_cmd, output_dir)
    if args.mode in ("run", "both"):
        run_analysis.run(arch_name, arch_info, link_cmd, output_dir)

if __name__ == "__main__":
    main()
