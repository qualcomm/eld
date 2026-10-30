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
import itertools
from pathlib import Path
import sys

import run_analysis
import static_analysis

import arch.arm.arch
import arch.x86_64.arch

ARCH = {
    "arm": arch.arm.arch.ARCH,
    "x86_64": arch.x86_64.arch.ARCH,
}

MODES = ["static", "run", "both"]

PARAMETERS = {
    "symbol_binding": ["global", "local", "weak"],
    "symbol_visibility": ["default", "hidden"],
}

REPORT_PARAMETERS = ["symbol_binding", "symbol_visibility"]

REPORT_PARAMETER_NAMES = {
    "symbol_binding": "Binding",
    "symbol_visibility": "Visibility",
}

def create_argparser():
    argparser = argparse.ArgumentParser()
    argparser.add_argument("--arch",
                           dest="arch",
                           required=True,
                           help=f"Arch, one of [{','.join(ARCH.keys())}]")
    argparser.add_argument("--output-dir",
                           dest="output_dir",
                           required=True,
                           help="Output directory")
    argparser.add_argument("--mode",
                           dest="mode",
                           choices=MODES,
                           default="both",
                           help="Which testing mode(s) to run (default: both)")
    argparser.add_argument("-v", "--verbose",
                           dest="verbose",
                           action="store_true",
                           help="Print commands and command output")
    return argparser

def main():
    args = create_argparser().parse_args()
    output_dir = args.output_dir

    if not args.arch in ARCH:
        print(f"Don't know anything about architecture {args.arch}",
              file=sys.stderr)
        sys.exit(1)

    arch_name = args.arch
    arch_info = ARCH[arch_name]

    success = True
    if args.mode in ("static", "both"):
        success = static_analysis.run(arch_name, arch_info, output_dir) and success
    if args.mode in ("run", "both"):
        success = run_run_analysis(arch_name, arch_info, output_dir,
                                   args.verbose) and success

    sys.exit(0 if success else 1)


def run_run_analysis(arch_name, arch_info, output_dir, verbose):
    output_path = Path(output_dir) / "run"
    output_path.mkdir(parents=True, exist_ok=True)

    config = arch_info["config"]

    results_by_report = {}
    for combination in build_run_matrix(arch_info):
        report_name = parameter_report_name(combination)
        report_dir = output_path / report_name
        report_dir.mkdir(parents=True, exist_ok=True)
        result = run_analysis.run(arch_name, config, combination, report_name,
                                  report_dir, verbose)
        results_by_report.setdefault(report_name, []).append(result)

    all_passed = True
    for report_name, results in results_by_report.items():
        table = format_run_report(report_name, results)
        (output_path / report_name / "report.md").write_text(table)
        print(table)
        all_passed = all_passed and all(run_analysis.result_passed(r)
                                        for r in results)
    return all_passed


def build_run_matrix(arch_info):
    for parameters in build_parameter_matrix(PARAMETERS):
        for reloc in arch_info["relocs"]:
            yield {**parameters, "relocation": reloc}


def build_parameter_matrix(parameters):
    names = list(parameters.keys())
    for values in itertools.product(*(parameters[name] for name in names)):
        yield dict(zip(names, values))


def parameter_report_name(combination):
    return "-".join(
        f"{REPORT_PARAMETER_NAMES[name]}{format_parameter_value(combination[name])}"
        for name in REPORT_PARAMETERS)


def format_parameter_value(value):
    return str(value).replace("_", " ").title().replace(" ", "")


def format_run_report(report_name, results):
    lines = [f"# {report_name}", "",
             "| Relocation | Build | exit_code_check |",
             "| --- | --- | --- |"]
    for result in results:
        invariant = result.invariant_results[0]
        invariant_text = "PASS" if invariant.passed else invariant.detail
        if not invariant_text:
            invariant_text = "FAIL"
        lines.append("| " + " | ".join([
            result.relocation,
            result.build_status,
            invariant_text,
        ]) + " |")
    passed = sum(1 for result in results if run_analysis.result_passed(result))
    failed = len(results) - passed
    lines += ["", f"{passed} relocation passed, {failed} relocation failed", ""]
    return "\n".join(lines)

if __name__ == "__main__":
    main()
