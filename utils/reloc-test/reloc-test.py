#!/usr/bin/env python3

import argparse
import sys
import os
import subprocess
import yaml
import re
from elftools.elf.elffile import ELFFile

import arch.aarch64.arch

ARCH = {
    "aarch64": arch.aarch64.arch.ARCH,
}

TIMEOUT = 5
DATA_DIR = os.path.abspath(os.path.dirname(__file__))
YAML2OBJ = ["yaml2obj"]  # Must be in $PATH
DEFAULT_LINK = ["ld.lld"]  # Must be in $PATH
DEFAULT_TOOLS_PREFIX = "llvm-"


def make_obj(arch):
    return {
        "FileHeader": {
            "Class": "ELFCLASS" + str(arch["bits"]),
            "Data": "ELFDATA2LSB",
            "Type": "ET_REL",
            "Machine": arch["mach"],
        },
        "Sections": [],
        "Symbols": [],
    }


def make_text_section(arch):
    return {
        "Name": ".text",
        "Type": "SHT_PROGBITS",
        "Flags": ["SHF_ALLOC", "SHF_EXECINSTR"],
        "Content": "ff" * (arch["bits"] // 8),
    }


def make_data_section(arch):
    return {
        "Name": ".data",
        "Type": "SHT_PROGBITS",
        "Flags": ["SHF_ALLOC", "SHF_WRITE"],
        "Content": "ff" * (arch["bits"] // 8),
    }


def make_rel_section(arch, section):
    name = section["Name"]
    return {
        "Name": "." + arch["rel"] + name,
        "Type": "SHT_" + arch["rel"].upper(),
        "Flags": ["SHF_INFO_LINK"],
        "Link": ".symtab",
        "Info": name,
        "Relocations": [],
    }


def add_symbol(obj, section, name, type, bind):
    obj["Symbols"] += [{
        "Name": name,
        "Type": f"STT_{type.upper()}",
        "Binding": f"STB_{bind.upper()}",
        "Section": section,
        "Value": 4,
    }]


def add_undef_symbol(arch, obj, rel_sec, name, reloc, binding="STB_GLOBAL"):
    obj["Symbols"] += [{
        "Name": name,
        "Binding": binding,
    }]
    rel_sec["Relocations"] += [{
        "Symbol": name,
        "Offset": 0,
        "Type": arch["reloc_prefix"] + "_" + reloc,
    }]


def write_yaml(o, f):
    f.write("--- !ELF\n")
    yaml.dump(o, f, default_flow_style=False)
    f.write("---\n")


def create_obj(y, base_name):
    Y = os.path.join(output_dir, f"{base_name}.yaml")
    with open(Y, "w") as f:
        write_yaml(y, f)
    with open(Y, "r") as f:
        subprocess.check_call(
            YAML2OBJ + ["-o", os.path.join(output_dir, f"{base_name}.o")],
            stdin=f,
            universal_newlines=True)


def dump(arch, b, c=0):
    s = []
    if c == 0:
        c = arch["bits"] // 8
    while len(b) != 0:
        s += [int.from_bytes(b[:c], arch["endianness"])]
        b = b[c:]
    return s


# These matchers come in the order of priority, first one wins.
ERR_STATUS_PATTERN = [
    (i, re.compile(j)) for i, j in [
        ("UNKNOWN_RELOC", "Invalid (.+) reloc number"),  # ld
        ("UNKNOWN_RELOC", "unrecognized relocation"),  # ld
        ("UNKNOWN_RELOC", "unsupported relocation"),  # ld
        ("UNKNOWN_RELOC", "unknown relocation"),  # lld
        ("UNKNOWN_RELOC", "Unsupported relocation"),  # eld
        ("UNKNOWN_RELOC", "Unknown relocation"),  # eld
        ("STATIC_DYNAMIC", "static link of dynamic object"),  # ld, lld
        ("STATIC_DYNAMIC", "shared objects with -static option"),  # eld
        ("NON_PIC", "not be used when making a shared object"),  # ld, lld
        ("NON_PIC", "recompile with -fPIC"),  # lld
        ("NON_TLS", "used with non-TLS symbol"),  # ld, lld
    ]
]


def match_status(messages):
    for substatus, pattern in ERR_STATUS_PATTERN:
        for s in messages:
            if pattern.search(s):
                return substatus
    return ""


ERR_HIDE_PATTERN = [
    re.compile(i) for i in [
        "Warning: ",  # eld warning
        "^ *[0-9]+",  # lld backtrace
    ]
]


def should_hide(s):
    for pattern in ERR_HIDE_PATTERN:
        if pattern.search(s):
            return True
    return False


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
    argparser.add_argument("--tools-prefix",
                           dest="tools_prefix",
                           help=f"Tools prefix e.g. `{DEFAULT_TOOLS_PREFIX}`")
    return argparser


args = create_argparser().parse_args()
output_dir = args.output_dir if args.output_dir else "."
link_cmd = args.link_cmd.split(
) if args.link_cmd else DEFAULT_LINK  # TODO: spaces and quotes are probably not handled well.
tools_prefix = args.tools_prefix if args.tools_prefix else DEFAULT_TOOLS_PREFIX

if not args.arch in ARCH:
    print(f"Don't know anything about architecture {args.arch}",
          file=sys.stderr)
    sys.exit(1)

arch = args.arch

# Type of output executable -> linker options.
EXEC_TYPE = {
    "static": ["-static"],
    "dynamic": ["-Bdynamic"],
    "shared": ["-shared"],
    "pie": ["-pie"],
}

# Type of symbol -> (function to create section, function to create this symbol, linker options)
SYM_TYPE = {
    "undef": (make_data_section, lambda obj, rel_sec, reloc: add_undef_symbol(
        ARCH[arch], obj, rel_sec, "foo", reloc, "STB_WEAK"), []),
    "dyndat": (make_data_section, lambda obj, rel_sec, reloc: add_undef_symbol(
        ARCH[arch], obj, rel_sec, "object", reloc), [f"lib.{arch}.so"]),
    "dynfun": (make_text_section, lambda obj, rel_sec, reloc: add_undef_symbol(
        ARCH[arch], obj, rel_sec, "func", reloc), [f"lib.{arch}.so"]),
}

PRINT_TAGS = ["DT_FLAGS_1", "DT_PLTGOT", "DT_PLTRELSZ", "DT_PLTREL"]

TOOLS = {
    "objdump": [tools_prefix + "objdump", "-dsx"],
    "readelf": [tools_prefix + "readelf", "-a"],
}

# Create the shared library to link to.
base_name = f"lib.{arch}"
obj = make_obj(ARCH[arch])
for i, make_section in [("object", make_data_section),
                        ("func", make_text_section)]:
    sec = make_section(ARCH[arch])
    rel_sec = make_rel_section(ARCH[arch], sec)
    obj["Sections"] += [sec, rel_sec]
    add_symbol(obj, sec["Name"], i, i, "global")
create_obj(obj, base_name)
subprocess.check_call(DEFAULT_LINK +
                      [f"{base_name}.o", "-shared", "-o", f"{base_name}.so"],
                      cwd=output_dir)

#  Main loop.
for exec_type, exec_cmd in EXEC_TYPE.items():
    for sym_type, (make_section, obj_func, lib_cmd) in SYM_TYPE.items():
        for rel_type in ARCH[arch]["relocs"]:
            name = f"{exec_type}.{sym_type}.{rel_type.lower()}"
            obj = make_obj(ARCH[arch])
            sec = make_section(ARCH[arch])
            rel_sec = make_rel_section(ARCH[arch], sec)
            obj["Sections"] += [sec, rel_sec]
            obj_func(obj, rel_sec, rel_type)
            create_obj(obj, f"a.{name}")

            O = f"a.{name}.out"
            cmd = link_cmd + exec_cmd + [
                "-o", O, "-T",
                os.path.join(DATA_DIR, "script.t"), "-e", "0",
                f"-Map=" + f"a.{name}.map", f"a.{name}.o"
            ] + lib_cmd
            with open(os.path.join(output_dir, f"a.{name}.cmd"),
                      "w") as cmd_file:
                print(" ".join(cmd), file=cmd_file)
            try:
                ret = subprocess.run(cmd,
                                     stderr=subprocess.PIPE,
                                     cwd=output_dir,
                                     text=True,
                                     timeout=TIMEOUT)
                status = "FAIL" if ret.returncode else "OK"
            except subprocess.TimeoutExpired:
                status = "TIMEOUT"
            messages = ret.stderr.splitlines()
            substatus = match_status(messages)

            print()
            print(f"RUN {name}")
            print(f"STATUS {status}")
            if substatus:
                print(f"ERROR {substatus}")
            else:
                for i in messages:
                    if not should_hide(i):
                        print(i)

            with open(os.path.join(output_dir, f"a.{name}.err"),
                      "w") as stderr:
                stderr.write(ret.stderr)

            if status != "OK":
                continue

            with open(os.path.join(output_dir, O), "rb") as f:
                elf = ELFFile(f)

                # TODO: Build a map: interval -> section.
                def section_by_offset(offset):
                    for i in range(elf.num_sections()):
                        s = elf.get_section(i)
                        section_addr = s["sh_addr"]
                        section_size = s["sh_size"]
                        if (s['sh_type'] != 'SHT_NOBITS'
                                and offset >= section_addr
                                and offset < section_addr + section_size):
                            return i
                    return None

                dynsym = None
                plt_index = None
                got_index = None
                gotplt_index = None
                output_section_index = None
                for i in range(elf.num_sections()):
                    s = elf.get_section(i)
                    if s.name == ".dynsym":
                        dynsym = elf.get_section(i)
                    if s.name == ".plt":
                        plt_index = i
                    if s.name == ".got":
                        got_index = i
                    if s.name == ".got.plt":
                        gotplt_index = i
                    if s.name == sec["Name"]:
                        output_section_index = i

                dyn_rels = {}
                for segment in elf.iter_segments():
                    if segment.header.p_type == "PT_DYNAMIC":
                        # Print dynamic tags.
                        tags = {
                            tag.entry.d_tag: tag.entry.d_val
                            for tag in segment.iter_tags()
                        }
                        for i in PRINT_TAGS:
                            if i in tags:
                                print(i, "0x{:x}".format(tags[i]))

                        # Read dynamic relocations.
                        # TODO: Proper way to map relocation table to its symbol section?
                        for t_name, t in segment.get_relocation_tables().items(
                        ):
                            for i in range(t.num_relocations()):
                                r = t.get_relocation(i)
                                offset = r["r_offset"]
                                print(
                                    f"DYNREL {t_name}[{i}] offset 0x{offset:x}"
                                )
                                section_index = section_by_offset(offset)
                                if section_index == None:
                                    print(
                                        f"{name}: no section for relocation {t_name}[{i}] offset {offset}",
                                        file=sys.stderr)
                                    continue
                                section = elf.get_section(section_index)
                                section_offset = offset - section["sh_addr"]
                                print(
                                    f"  {section.name} @0x{section_offset:x}")
                                dyn_rels[(section_index, section_offset)] = r

                # Print PLT.
                if plt_index:
                    plt = elf.get_section(plt_index)
                    sz = ARCH[arch]["pltsz"]  # bytes
                    d = [
                        dump(ARCH[arch],
                             plt.data()[i:i + sz], ARCH[arch]["instr_bytes"])
                        for i in range(0, len(plt.data()), sz)
                    ]
                    for i in range(len(d)):
                        print(plt.name, f"[{i}]",
                              '[{}]'.format(', '.join(hex(j) for j in d[i])))

                # Print GOT slots.
                word_bytes = ARCH[arch]["bits"] // 8
                for section_index in [
                        i for i in [got_index, gotplt_index] if i
                ]:
                    section = elf.get_section(section_index)
                    d = dump(ARCH[arch], section.data(), word_bytes)
                    section_offset = 0
                    for i in range(len(d)):
                        print(section.name, f"[{i}] 0x{d[i]:x}")
                        r = dyn_rels.get((section_index, section_offset))
                        if r:
                            print(
                                "  ",
                                dynsym.get_symbol(r["r_info_sym"]).name,
                                ARCH[arch]["dyn_relocs"].get(
                                    r["r_info_type"],
                                    str(r["r_info_type"])), r["r_addend"])
                        section_offset += word_bytes

                # Dump relocation values. The code below assumes there is only one input section in the output section.
                if output_section_index:
                    output_section = elf.get_section(output_section_index)
                    for rel in rel_sec["Relocations"]:
                        sec_name = output_section.name
                        offset = rel["Offset"]  # This is input section offset.
                        value = int.from_bytes(
                            output_section.data()[offset:offset + word_bytes],
                            ARCH[arch]["endianness"])
                        print(f"{sec_name} @0x{offset:x} 0x{value:x}")
                        r = dyn_rels.get((output_section_index, offset))
                        if r:
                            print(
                                "  ",
                                dynsym.get_symbol(r["r_info_sym"]).name,
                                ARCH[arch]["dyn_relocs"].get(
                                    r["r_info_type"],
                                    str(r["r_info_type"])), r["r_addend"])

                # Run additional tools on the binary for extra bonus.
                for tool, cmd in TOOLS.items():
                    with open(os.path.join(output_dir, '.'.join([O, tool])),
                              "wb") as stdout:
                        subprocess.run(cmd + [O],
                                       cwd=output_dir,
                                       stdout=stdout)
