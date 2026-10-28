"""Check stored values and simulate the relevant Hexagon loader fixups."""

import argparse
import json
from pathlib import Path
import subprocess
import sys


parser = argparse.ArgumentParser()
parser.add_argument("--readelf", required=True)
parser.add_argument("--image", type=Path, required=True)
parser.add_argument("--ordinary", action="store_true")
parser.add_argument("--only-absolute", action="store_true")
parser.add_argument("--no-combreloc", action="store_true")
parser.add_argument("--layout-reference", type=Path)
parser.add_argument("--preemptible", action="store_true")
args = parser.parse_args()
info = json.loads(subprocess.check_output([
    args.readelf, "--elf-output-style=JSON", "--symbols", "--relocations",
    "--sections", "--dynamic-table", str(args.image)
], text=True))[0]
sections = {s["Section"]["Name"]["Name"]: s["Section"]
            for s in info["Sections"]}
section_names = {s["Section"]["Index"]: s["Section"]["Name"]["Name"]
                 for s in info["Sections"]}
symbols = {s["Symbol"]["Name"]["Name"]: s["Symbol"]
           for s in info["Symbols"]}
dynamic = {d["Type"]: d["Value"] for d in info.get("DynamicSection", [])}
relocs = [r["Relocation"] for g in info.get("Relocations", [])
          if section_names[g["SectionIndex"]] == ".rela.dyn"
          for r in g["Relocs"]]
data = args.image.read_bytes()
if args.layout_reference:
    reference = json.loads(subprocess.check_output([
        args.readelf, "--elf-output-style=JSON", "--sections",
        str(args.layout_reference)
    ], text=True))[0]
    reference_sections = {s["Section"]["Name"]["Name"]: s["Section"]
                          for s in reference["Sections"]}
    for name, section in sections.items():
        if section["Flags"]["Value"] & 2:
            before = reference_sections[name]
            assert (section["Address"], section["Offset"]) == (
                before["Address"], before["Offset"]), (name, section, before)
mask = 0xffffffff
assigned = symbols["data"]["Value"]
if not args.ordinary:
    assert assigned == 0x12345678, symbols["data"]
    assert symbols["data"]["Section"]["Name"] == "Absolute"


def word(address):
    for section in sections.values():
        start = section["Address"]
        if (section["Flags"]["Value"] & 2 and
                start <= address and address + 4 <= start + section["Size"]):
            pos = section["Offset"] + address - start
            return int.from_bytes(data[pos:pos + 4], "little")
    raise AssertionError(("unmapped address", hex(address)))


if "RELASZ" in dynamic:
    assert dynamic["RELASZ"] == len(relocs) * 12, (dynamic, relocs)
    assert sections[".rela.dyn"]["Size"] == dynamic["RELASZ"]
    relative_count = sum(r["Type"]["Name"] == "R_HEX_RELATIVE" for r in relocs)
    if args.no_combreloc:
        relative_count = 0
        for r in relocs:
            if r["Type"]["Name"] != "R_HEX_RELATIVE":
                break
            relative_count += 1
    assert dynamic["RELACOUNT"] == relative_count, (dynamic, relocs)
    assert all(r["Type"]["Name"] == "R_HEX_RELATIVE"
               for r in relocs[:relative_count]), relocs
else:
    assert not relocs, relocs

if not args.only_absolute and dynamic:
    if dynamic.get("FLAGS_1", 0) & 0x08000000:
        # Definitions in a PIE bind locally, unlike default-visible globals
        # in a shared library. Their ordinary pointers still need load bias.
        assert any(r["Type"]["Name"] == "R_HEX_RELATIVE" and
                   r["Offset"] == symbols["external_pointer"]["Value"]
                   for r in relocs), relocs
    else:
        assert any(r["Type"]["Name"] == "R_HEX_GLOB_DAT" and
                   r["Symbol"]["Name"] == "external" for r in relocs), relocs
        assert any(r["Type"]["Name"] == "R_HEX_32" and
                   r["Symbol"]["Name"] == "external" for r in relocs), relocs
    normal_address = symbols["normal_pointer"]["Value"]
    assert any(r["Type"]["Name"] == "R_HEX_RELATIVE" and
               r["Offset"] == normal_address for r in relocs), relocs
elif args.only_absolute:
    assert not relocs, relocs

if args.preemptible:
    # setSymbolAddress must not implicitly disable interposition. The loader
    # still resolves this default-visible symbol for every pointer/GOT fixup.
    references = [r for r in relocs if r["Symbol"]["Name"] == "data"]
    assert sum(r["Type"]["Name"] == "R_HEX_GLOB_DAT" for r in references) == 1, references
    pointers = [r for r in references if r["Type"]["Name"] == "R_HEX_32"]
    assert len(pointers) == 3, pointers
    assert {r["Addend"] & 0xffffffff for r in pointers} == {0, 5, 0xffffffff}, pointers
    sys.exit(0)

pointer_address = symbols["pointers"]["Value"]
expected = {pointer_address + 4 * i: (assigned + offset) & mask
            for i, offset in enumerate((0, 5, -1))}
# The first reference reserves data's GOT slot before external's slot.
got = sections[".got"]
assert got["Size"] == (4 if args.only_absolute else 8), got
expected[got["Address"]] = assigned
assert all(word(address) == value for address, value in expected.items()), expected
if not args.ordinary:
    assert not any(r["Offset"] in expected for r in relocs), relocs

for bias in (0, 0x1000, 0x40000000):
    memory = {address: word(address) for address in expected}
    if "normal_pointer" in symbols:
        normal_pointer = symbols["normal_pointer"]["Value"]
        memory[normal_pointer] = word(normal_pointer)
    for r in relocs:
        if r["Type"]["Name"] == "R_HEX_RELATIVE":
            memory[r["Offset"]] = (bias + r["Addend"]) & mask
    wanted = {address: (value + (bias if args.ordinary else 0)) & mask
              for address, value in expected.items()}
    assert all(memory[address] == value for address, value in wanted.items()), (
        hex(bias), memory, wanted, relocs)
    if "normal_pointer" in symbols and dynamic:
        assert memory[normal_pointer] == (symbols["normal"]["Value"] + bias) & mask
