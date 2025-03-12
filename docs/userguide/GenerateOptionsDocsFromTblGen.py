#!/usr/bin/env

import argparse
import sys
import json
import re


class Option:
    """Option represents a command-line option"""

    def __init__(self, raw_info):
        self.name = raw_info["Name"]
        # Prefixes can be single-dash(-), double dash(--), ...
        self.prefixes = raw_info["Prefixes"]
        self.group = raw_info["Group"]["def"] if raw_info["Group"] else None
        self.help_text = raw_info["HelpText"]
        if self.help_text:
            self.help_text = Option.fixHelpTextFormatting(self.help_text)
        # Meta var name is used to represent the option value.
        # For example: -Map <map-file>; here '<map-file>' is the meta var name.
        self.meta_var_name = (
            raw_info["MetaVarName"] if raw_info["MetaVarName"] else "<value>"
        )
        # Separate argument example: -Map <out-file>
        self.has_separate_argument = "Separate" in raw_info["!superclasses"]
        # Joined argument example: -Map=<out-file>
        self.has_joined_argument = "Joined" in raw_info["!superclasses"]
        self.alias = raw_info["Alias"]["def"] if raw_info["Alias"] else None

        # List of options that are aliased to this canonical option.
        #
        # Aliases are only inserted for canonical options.
        # If there are two options -non_shared and -static, and -non_shared
        # is aliased to -static, then we say that -static is the
        # canonical option because it is not aliased to any other option --
        # other options (such as -non_shared) are aliased to canonical
        # options (such as -static)
        self.aliases = []

    @classmethod
    def is_option(cls, raw_info):
        return (
            isinstance(raw_info, dict)
            and "!superclasses" in raw_info
            and "Option" in raw_info["!superclasses"]
        )

    def is_canonical(self):
        """
        Canonical options are the ones which not aliased to any other option.
        Please note that other options may be aliased to canonical options.
        """
        return not self.alias

    def get_all_option_forms(self):
        """
        Return all forms of the options. An option may have several forms.
        For example: -Map <file>, --Map <file>, -Map=<file>, --Map=<file>, ...
        """
        option_forms = []
        for prefix in self.prefixes:
            form = "{prefix}{name}".format(prefix=prefix, name=self.name)
            if self.has_separate_argument:
                form += " {}".format(self.meta_var_name)
            elif self.has_joined_argument:
                form += "{}".format(self.meta_var_name)
            option_forms.append(form)

        for alias in self.aliases:
            alias_option_forms = alias.get_all_option_forms()
            option_forms.extend(alias_option_forms)

        return option_forms

    def generate_docs(self, out):
        option_forms = self.get_all_option_forms()
        if not option_forms:
            return
        out.write(".. option:: " + ", ".join(option_forms))
        out.write("\n")
        if self.help_text:
            out.write("\n")
            out.write(self.help_text)
            out.write("\n")

    @classmethod
    def fixHelpTextFormatting(self, help_text):
        """
        Modifies help text formatting to be as per restructureText expectations.
        One of the complication transformations it performs is transforming
        documentation such as:

        -trace=linker-script
        -trace=files
        -trace=LTO

        to:

        * -trace=linker-script
        * -trace=files
        * -trace=LTO

        This transformation is required to improve formatting in the sphinx
        documentation output.
        """
        if re.search(r"^[ \t]+-", help_text, re.MULTILINE):
            help_text = help_text.replace("\n", "\n\n", 1)
        help_text = re.sub(r"^[ \t]+-", r"* \g<0>", help_text, 0, re.MULTILINE)
        help_text = help_text.replace("\t", "")
        help_text = " " * 3 + help_text.replace("\n", "\n" + " " * 3)
        return help_text


class OptionGroup:
    def __init__(self, raw_info):
        if raw_info is None:
            self.name = ""
            self.help_text = ""
            self.options = []
            return
        self.name = raw_info["Name"]
        self.help_text = raw_info["HelpText"]
        self.options = []

    @classmethod
    def is_option_group(cls, raw_info) -> bool:
        return (
            isinstance(raw_info, dict)
            and "!superclasses" in raw_info
            and "OptionGroup" in raw_info["!superclasses"]
        )

    def generate_docs(self, out):
        group_line = self.help_text + "\n"
        out.write(group_line)
        out.write("^" * len(group_line) + "\n")


def create_groups(raw_options_info):
    """
    Read raw json-dump of documentation TableGen file and creates
    all the Group objects.
    """
    groups = {}
    groups[None] = OptionGroup(None)
    for key, raw_info in raw_options_info.items():
        if not OptionGroup.is_option_group(raw_info):
            continue
        groups[key] = OptionGroup(raw_info)
    return groups


def create_options(raw_options_info):
    """
    Read raw json-dump of documentation TableGen file and creates
    all the Option objects.
    """
    options = {}
    for key, raw_info in raw_options_info.items():
        if not Option.is_option(raw_info):
            continue
        options[key] = Option(raw_info)
    return options


def set_aliases(options):
    def find_canonical_option(option):
        if option.alias:
            return option
        return find_canonical_option(options[option.alias])

    for key, option_info in options.items():
        if not option_info.alias:
            continue
        canonical_option = find_canonical_option(option_info)
        canonical_option.aliases.append(option_info)


def add_canonical_options_to_group(groups, options):
    """
    Adds all the canonical options to option groups.
    """
    for key, option_info in options.items():
        if not option_info.is_canonical():
            continue
        group_info = groups[option_info.group]
        group_info.options.append(option_info)


def get_refined_options_info(raw_options_info):
    """
    Reads raw json dump of options and creates a refined easy-to-use
    Option and OptionGroup objects.
    """
    groups = create_groups(raw_options_info)
    options = create_options(raw_options_info)
    set_aliases(options)
    add_canonical_options_to_group(groups, options)
    return groups


def generate_docs(groups, out):
    """Generate command-line options documentation"""
    for key, group_info in groups.items():
        if not group_info.options or key is None:
            continue
        group_info.generate_docs(out)
        for option_info in group_info.options:
            option_info.generate_docs(out)
            out.write("\n")
        out.write("\n\n")


def create_argparser():
    parser = argparse.ArgumentParser(
        prog="GenerateOptionsDocsFromTblGen",
        description=(
            "Utility script for generating sphinx restructureText "
            "documentation for command-line options from JSON-dump of "
            "command-line options TableGen file."
        ),
    )

    parser.add_argument(
        "options_json_dump",
        metavar="json-dump-file",
        help="JSON dump file of the command-line options TableGen file",
    )

    parser.add_argument("-o", "--output", help="output file")
    parser.add_argument(
        "-s",
        "--skip",
        help="Options present in the specified json dump file are skipped / not documented",
    )

    return parser

def remove_groups(groups, skip_options_groups):
    for key, group_info  in skip_options_groups.items():
        if key in groups:
            del groups[key]

if __name__ == "__main__":
    out = sys.stdout
    parser = create_argparser()
    args = parser.parse_args()
    skip_options_groups = None
    if args.skip:
        with open(args.skip) as skip_options_json_dump_file:
            raw_skip_options_info = json.load(skip_options_json_dump_file)
            skip_options_groups = get_refined_options_info(raw_skip_options_info)

    with open(args.options_json_dump) as options_json_dump_file:
        raw_options_info = json.load(options_json_dump_file)
        groups = get_refined_options_info(raw_options_info)
        if skip_options_groups:
            remove_groups(groups, skip_options_groups)
    # If output option is not specified then write to the standard output
    # stream.
    out = sys.stdout
    if args.output:
        out = open(args.output, "w")
    generate_docs(groups, out)
    if args.output:
        out.close()
