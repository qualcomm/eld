#!/usr/bin/env python3

import argparse
from pathlib import Path
import shutil
import sys


def concatenate(output_path, input_paths):
    with Path(output_path).open("wb") as output:
        for input_path in input_paths:
            with Path(input_path).open("rb") as input_file:
                shutil.copyfileobj(input_file, output)


def run_cat(args):
    concatenate(args.output, args.inputs)


def main(argv):
    parser = argparse.ArgumentParser(description="Utilities for ELD lit tests")
    subparsers = parser.add_subparsers(dest="command", required=True)

    cat_parser = subparsers.add_parser(
        "cat", help="concatenate input files into an output file"
    )
    cat_parser.add_argument("output")
    cat_parser.add_argument("inputs", nargs="+")

    args = parser.parse_args(argv)
    commands = {"cat": run_cat}
    try:
        commands[args.command](args)
    except OSError as error:
        parser.error(str(error))

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
